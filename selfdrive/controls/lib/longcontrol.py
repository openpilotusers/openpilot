from cereal import car, log
from common.numpy_fast import clip, interp
from selfdrive.controls.lib.pid import PIController

from selfdrive.controls.lib.events import Events
from common.params import Params

import common.log as trace1
import common.CTime1000 as tm

LongCtrlState = log.ControlsState.LongControlState

STOPPING_EGO_SPEED = 0.5
MIN_CAN_SPEED = 0.3  # TODO: parametrize this in car interface
STOPPING_TARGET_SPEED = MIN_CAN_SPEED + 0.01
STARTING_TARGET_SPEED = 0.05
BRAKE_THRESHOLD_TO_PID = 0.5

STOPPING_BRAKE_RATE = 0.5  # brake_travel/s while trying to stop
STARTING_BRAKE_RATE = 6  # brake_travel/s while releasing on restart
BRAKE_STOPPING_TARGET = 0.5  # apply at least this amount of brake to maintain the vehicle stationary

RATE = 100.0


def long_control_state_trans(active, long_control_state, v_ego, v_target, v_pid,
                             output_gb, brake_pressed, cruise_standstill, brake_hold, gas_pressed):
  """Update longitudinal control state machine"""
  stopping_condition = (v_ego < 1.3 and cruise_standstill) or \
                       (v_ego < STOPPING_EGO_SPEED and
                        ((v_pid < STOPPING_TARGET_SPEED and v_target < STOPPING_TARGET_SPEED) or
                        brake_pressed))

  starting_condition = v_target > STARTING_TARGET_SPEED and not cruise_standstill and (int(Params().get('OpkrAutoResume')) == 1)

  if not active:
    long_control_state = LongCtrlState.off

  else:
    if long_control_state == LongCtrlState.off:
      if active:
        long_control_state = LongCtrlState.pid

    elif long_control_state == LongCtrlState.pid:
      if stopping_condition:
        long_control_state = LongCtrlState.stopping

    elif long_control_state == LongCtrlState.stopping:
      if starting_condition:
        long_control_state = LongCtrlState.starting

    elif long_control_state == LongCtrlState.starting:
      if stopping_condition:
        long_control_state = LongCtrlState.stopping
      elif output_gb >= -BRAKE_THRESHOLD_TO_PID:
        long_control_state = LongCtrlState.pid

  return long_control_state


class LongControl():
  def __init__(self, CP, compute_gb):
    self.long_control_state = LongCtrlState.off  # initialized to off
    self.pid = PIController((CP.longitudinalTuning.kpBP, CP.longitudinalTuning.kpV),
                            (CP.longitudinalTuning.kiBP, CP.longitudinalTuning.kiV),
                            rate=RATE,
                            sat_limit=0.8,
                            convert=compute_gb)
    self.v_pid = 0.0
    self.last_output_gb = 0.0
    self.long_stat = ""

  def reset(self, v_pid):
    """Reset PID controller and change setpoint"""
    self.pid.reset()
    self.v_pid = v_pid

  def update(self, active, CS, v_target, v_target_future, a_target, CP):
    """Update longitudinal control. This updates the state machine and runs a PID loop"""
    # Actuation limits
    gas_max = interp(CS.vEgo, CP.gasMaxBP, CP.gasMaxV)
    brake_max = interp(CS.vEgo, CP.brakeMaxBP, CP.brakeMaxV)

    # Update state machine
    output_gb = self.last_output_gb
    self.long_control_state = long_control_state_trans(active, self.long_control_state, CS.vEgo,
                                                       v_target_future, self.v_pid, output_gb,
                                                       CS.brakePressed, CS.cruiseState.standstill, CS.brakeHold, CS.gasPressed)

    v_ego_pid = max(CS.vEgo, MIN_CAN_SPEED)  # Without this we get jumps, CAN bus reports 0 when speed < 0.3

    if self.long_control_state == LongCtrlState.off or CS.gasPressed:
      self.reset(v_ego_pid)
      output_gb = 0.

    # tracking objects and driving
    elif self.long_control_state == LongCtrlState.pid:
      self.v_pid = v_target
      self.pid.pos_limit = gas_max
      self.pid.neg_limit = - brake_max

      # Toyota starts braking more when it thinks you want to stop
      # Freeze the integrator so we don't accelerate to compensate, and don't allow positive acceleration
      prevent_overshoot = not CP.stoppingControl and CS.vEgo < 1.5 and v_target_future < 0.7
      deadzone = interp(v_ego_pid, CP.longitudinalTuning.deadzoneBP, CP.longitudinalTuning.deadzoneV)

      output_gb = self.pid.update(self.v_pid, v_ego_pid, speed=v_ego_pid, deadzone=deadzone, feedforward=a_target, freeze_integrator=prevent_overshoot)

      if prevent_overshoot:
        output_gb = min(output_gb, 0.0)

    # Intention is to stop, switch to a different brake control until we stop
    elif self.long_control_state == LongCtrlState.stopping:
      # Keep applying brakes until the car is stopped
      if not CS.standstill or output_gb > -BRAKE_STOPPING_TARGET:
        output_gb -= STOPPING_BRAKE_RATE / RATE
      output_gb = clip(output_gb, -brake_max, gas_max)

      self.reset(CS.vEgo)

    # Intention is to move again, release brake fast before handing control to PID
    elif self.long_control_state == LongCtrlState.starting:
      if output_gb < -0.2:
        output_gb += STARTING_BRAKE_RATE / RATE
      self.reset(CS.vEgo)

    self.last_output_gb = output_gb
    final_gas = clip(output_gb, 0., gas_max)
    final_brake = -clip(output_gb, -brake_max, 0.)


    if self.long_control_state == LongCtrlState.stopping:
      self.long_stat = "STP"
      if v_target == 0:
        events = Events()
        events.add(car.CarEvent.EventName.standStill)
    elif self.long_control_state == LongCtrlState.starting:
      self.long_stat = "STR"
    elif self.long_control_state == LongCtrlState.pid:
      self.long_stat = "PID"
    elif self.long_control_state == LongCtrlState.off:
      self.long_stat = "OFF"
    else:
      self.long_stat = "---"

    str_log3 = 'LS={:s}  GS={:01.2f}/{:01.2f}  BK={:01.2f}/{:01.2f}  GB={:+04.2f}  TG=V:{:05.2f}/F:{:05.2f}/A:{:+04.2f}  GS={}'.format(self.long_stat, final_gas, gas_max, abs(final_brake), abs(brake_max), output_gb, v_target, abs(v_target_future), a_target, CS.gasPressed)
    trace1.printf2('{}'.format(str_log3))

    # 동작로직설명
    # self.long_control_state가 4가지 상태임. STP는 차가 정지상태, STR은 차가 막 출발하려고 하는 상태(앞차 출발할 경우), PID는 주행중 상태임
    # v_target과 주행 모델(cruise, mpc1..., model)값에 의해 gas/brake 값이 결정(PID제어, 기준값은 interface.py에서 불러옴)되며, output_gb값이 +이면 가속상태(gas>brake)이고, -이면 감속상태(brake>gas)임.
    # 현기 자체의 dynamic gas나 가변 브레이킹을 개발할 필요성이 있음. 간단한 테스트 결과, cut-in 감속등은 잘되고 있음을 확인했으며,
    # e2e기능(신호등 인식 감속, 정지선인식 감속, 속도표지판 인식 후 해당속도 주행 등)은 되든것 같은데 되는것 같지 않은 되는것 같은 듯.
    # 역시 고오오오급 개발자의 능력이 필요합니다^^ 질문은 사절하며, 이상 일반인의 헛소리였습니다. ※ 개발자 아님
    return final_gas, final_brake