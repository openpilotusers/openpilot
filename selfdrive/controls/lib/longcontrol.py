from cereal import car, log
from common.numpy_fast import clip, interp
from selfdrive.controls.lib.pid import LongPIDController

from selfdrive.config import Conversions as CV
from common.params import Params

import common.log as trace1
import common.CTime1000 as tm

LongCtrlState = log.ControlsState.LongControlState

STOPPING_EGO_SPEED = 0.3
MIN_CAN_SPEED = 0.3  # TODO: parametrize this in car interface
STOPPING_TARGET_SPEED = MIN_CAN_SPEED + 0.01
STARTING_TARGET_SPEED = 0.5
BRAKE_THRESHOLD_TO_PID = 0.2

STOPPING_BRAKE_RATE = 0.2  # brake_travel/s while trying to stop
STARTING_BRAKE_RATE = 0.8  # brake_travel/s while releasing on restart
BRAKE_STOPPING_TARGET = 0.7  # apply at least this amount of brake to maintain the vehicle stationary

RATE = 100.0


def long_control_state_trans(active, long_control_state, v_ego, v_target, v_pid,
                             output_gb, brake_pressed, cruise_standstill, stop, gas_pressed):
  """Update longitudinal control state machine"""
  stopping_condition = stop or (v_ego < 0.3 and cruise_standstill) or \
                       (v_ego < STOPPING_EGO_SPEED and \
                        ((v_pid < STOPPING_TARGET_SPEED and v_target < STOPPING_TARGET_SPEED) or
                        brake_pressed))

  starting_condition = v_target > STARTING_TARGET_SPEED and not cruise_standstill and (int(Params().get('OpkrAutoResume')) == 1 or gas_pressed)

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
    kdBP = [0., 2.237, 33, 55., 78]
    kdBP = [i * CV.MPH_TO_MS for i in kdBP]
    kdV = [1.5, 0.05, 0.4, 0.8, 1.2]
    self.pid = LongPIDController((CP.longitudinalTuning.kpBP, CP.longitudinalTuning.kpV),
                                 (CP.longitudinalTuning.kiBP, CP.longitudinalTuning.kiV),
                                 (kdBP, kdV),
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

  def dynamic_gas(self, v_ego, gas_button_status):
    dynamic = False
    if gas_button_status == 0: # NORMAL
      dynamic = True
      x = [0.0, 1.4082, 2.8031, 4.2266, 5.3827, 6.1656, 7.2478, 8.2831, 10.2447, 12.964, 15.423, 18.119, 20.117, 24.4661, 29.0581, 32.7101, 35.7633]
      y = [1.3, 1.304, 1.315, 1.342, 1.365, 1.386, 1.429, 1.454, 1.472, 1.48, 1.489, 1.421, 1.432, 1.480, 1.55, 1.621, 1.7]
    elif gas_button_status == 1: # SPORT
      y = [1.5, 1.85, 1.89]
    elif gas_button_status == 2: # ECO
      y = [1.25, 1.2, 1.35]

    if not dynamic:
      x = [0., 9., 55.]  # default BP values

    accel = interp(v_ego, x, y)

    min_return = 0.0
    max_return = 2.0
    return round(max(min(accel, max_return), min_return), 5)  # ensure we return a value between range

  def update(self, active, CS, v_target, v_target_future, a_target, CP, hasLead, radarState, gas_button_status):
    """Update longitudinal control. This updates the state machine and runs a PID loop"""
    # Actuation limits
    #gas_max = interp(CS.vEgo, CP.gasMaxBP, CP.gasMaxV)
    gas_max = self.dynamic_gas(CS.vEgo, gas_button_status)
    brake_max = interp(CS.vEgo, CP.brakeMaxBP, CP.brakeMaxV)

    # Update state machine
    output_gb = self.last_output_gb
    if radarState is None:
      dRel = 200
      vRel = 0
    else:
      dRel = radarState.leadOne.dRel
      vRel = radarState.leadOne.vRel
    if hasLead:
      stop = True if (dRel < 4.0 and radarState.leadOne.status) else False
    else:
      stop = False
    self.long_control_state = long_control_state_trans(active, self.long_control_state, CS.vEgo,
                                                       v_target_future, self.v_pid, output_gb,
                                                       CS.brakePressed, CS.cruiseState.standstill, stop, CS.gasPressed)

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

      if hasLead and radarState.leadOne.status and 5 < dRel < 23 and vRel < -3 and (CS.vEgo * CV.MS_TO_KPH) > (dRel+7) and output_gb < -0.5:
        output_gb -= 0.5
        output_gb = clip(output_gb, -brake_max, gas_max)

      if prevent_overshoot:
        output_gb = min(output_gb, 0.0)

    # Intention is to stop, switch to a different brake control until we stop
    elif self.long_control_state == LongCtrlState.stopping:
      # Keep applying brakes until the car is stopped
      factor = 1
      if hasLead:
        factor = interp(dRel,[2.0,3.0,4.0,5.0,6.0,7.0,8.0], [5,3,1.0,0.7,0.5,0.3,0.0])
      if not CS.standstill or output_gb > -BRAKE_STOPPING_TARGET:
        output_gb -= STOPPING_BRAKE_RATE / RATE * factor
      output_gb = clip(output_gb, -brake_max, gas_max)

      self.reset(CS.vEgo)

    # Intention is to move again, release brake fast before handing control to PID
    elif self.long_control_state == LongCtrlState.starting:
      factor = 1
      if hasLead:
        factor = interp(dRel,[0.0,2.0,3.0,4.0,6.0], [0.0,0.5,0.75,1.0,100.0])
      if output_gb < -0.2:
        output_gb += STARTING_BRAKE_RATE / RATE * factor
      self.reset(CS.vEgo)

    self.last_output_gb = output_gb
    final_gas = clip(output_gb, 0., gas_max)
    final_brake = -clip(output_gb, -brake_max, 0.)


    if self.long_control_state == LongCtrlState.stopping:
      self.long_stat = "STP"
    elif self.long_control_state == LongCtrlState.starting:
      self.long_stat = "STR"
    elif self.long_control_state == LongCtrlState.pid:
      self.long_stat = "PID"
    elif self.long_control_state == LongCtrlState.off:
      self.long_stat = "OFF"
    else:
      self.long_stat = "---"

    str_log3 = 'LS={:s}  GS={:01.2f}/{:01.2f}  BK={:01.2f}/{:01.2f}  GB={:+04.2f}  TG=P:{:05.2f}/V:{:05.2f}/F:{:05.2f}/A:{:+04.2f}  GS={}'.format(self.long_stat, final_gas, gas_max, abs(final_brake), abs(brake_max), output_gb, self.v_pid, v_target, abs(v_target_future), a_target, CS.gasPressed)
    trace1.printf2('{}'.format(str_log3))

    return final_gas, final_brake