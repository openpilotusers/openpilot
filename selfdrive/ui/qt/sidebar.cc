#include "common/util.h"
#include "sidebar.hpp"
#include "qt_window.hpp"

StatusWidget::StatusWidget(QString label, QString msg, QColor c, QWidget* parent) : QFrame(parent) {
  layout.setSpacing(0);

  if(msg.length() > 0){
    layout.setContentsMargins(50, 16, 16, 8);
    status.setAlignment(Qt::AlignLeft | Qt::AlignHCenter);
    status.setStyleSheet(R"(font-size: 65px; font-weight: 500;)");

    substatus.setAlignment(Qt::AlignLeft | Qt::AlignHCenter);
    substatus.setStyleSheet(R"(font-size: 30px; font-weight: 400;)");

    layout.addWidget(&status, 0, Qt::AlignLeft);
    layout.addWidget(&substatus, 0, Qt::AlignLeft);
  } else {
    layout.setContentsMargins(40, 24, 16, 24);

    status.setAlignment(Qt::AlignCenter);
    status.setStyleSheet(R"(font-size: 38px; font-weight: 500;)");
    layout.addWidget(&status, 0, Qt::AlignCenter);
  }

  update(label, msg, c);

  setMinimumHeight(124);
  setStyleSheet("background-color: transparent;");
  setLayout(&layout);
}

void StatusWidget::paintEvent(QPaintEvent *e){
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(QPen(QColor(0xb2b2b2), 3, Qt::SolidLine, Qt::FlatCap));
  // origin at 1.5,1.5 because qt issues with pixel perfect borders
  p.drawRoundedRect(QRectF(1.5, 1.5, size().width()-3, size().height()-3), 30, 30);

  p.setPen(Qt::NoPen);
  p.setBrush(color);
  p.setClipRect(0,0,25+6,size().height()-6,Qt::ClipOperation::ReplaceClip);
  p.drawRoundedRect(QRectF(6, 6, size().width()-12, size().height()-12), 25, 25);
}

void StatusWidget::update(QString label, QString msg, QColor c) {
  status.setText(label);
  substatus.setText(msg);

  if (color != c) {
    color = c;
    repaint();
  }
  return;
}

SignalWidget::SignalWidget(QString text, int strength, QWidget* parent) : QFrame(parent), _strength(strength) {
  layout.setMargin(0);
  layout.setSpacing(0);
  layout.insertSpacing(0, 40);

  label.setText(text);
  layout.addWidget(&label, 0, Qt::AlignLeft | Qt::AlignVCenter);
  label.setStyleSheet(R"(font-size: 35px; font-weight: 400;)");

  //atom
  // label_ip.setText(text);
  // label_ip.setStyleSheet(R"(font-size: 30px; font-weight: 580; color: #FFFF00;)");
  // layout.addWidget(&label_ip, 0, Qt::AlignCenter);

  image_bty.load("../assets/images/battery.png");

  setFixedWidth(177);
  setLayout(&layout);
}

void SignalWidget::paintEvent(QPaintEvent *e){
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(Qt::NoPen);
  p.setBrush(Qt::white);
  for (int i = 0; i < 5 ; i++){
    if(i >= _strength){
      p.setPen(Qt::NoPen);
      p.setBrush(Qt::darkGray);
    }
    p.drawEllipse(QRectF(_dotspace * i, _top, _dia, _dia));
  }

  //atom
  QRect  rect(90, _dia+18, 76, 36);
  QRect  bq(rect.left() + 6, rect.top() + 5, int((rect.width() - 19) * m_batteryPercent * 0.01), rect.height() - 11);
  QBrush bgBrush("#149948");
  p.fillRect(bq, bgBrush);  
  p.drawImage(rect, image_bty);

  p.setPen(Qt::white);
  QFont font = p.font();
  font.setPixelSize(25);
  p.setFont(font);

  char temp_value_str1[32];
  snprintf(temp_value_str1, sizeof(temp_value_str1), "%d%%", m_batteryPercent);
  p.drawText(rect, Qt::AlignCenter, temp_value_str1);
}

void SignalWidget::update(QString text, int strength, const UIScene &scene){
  int batteryPercent = scene.deviceState.getBatteryPercent();
  if( batteryPercent <= 0)
     batteryPercent = 0;

//  std::string ip = scene.deviceState.getWifiIpAddress();

  // QString txt(scene.ipAddr);
  // label_ip.setText(txt);

  int reDraw = 0;
  int battery_img = scene.deviceState.getBatteryStatus() == "Charging" ? 1 : 0;
  if( m_battery_img != battery_img )
  {
    reDraw = 1;    
    m_battery_img = battery_img;
    if( battery_img )
      image_bty.load("../assets/images/battery_charging.png");
    else
      image_bty.load("../assets/images/battery.png");
  }


  label.setText(text);
  if( _strength != strength )
  {
    reDraw = 1;    
    _strength = strength;
  }
  if( m_batteryPercent != batteryPercent )
  {
    reDraw = 1;    
    m_batteryPercent = batteryPercent;
  }
  
  if( reDraw )
  {
    repaint();
  }
}

// opkr
IPWidget::IPWidget(QString text, QWidget* parent) : QFrame(parent) {
  layout.setMargin(0);
  layout.setSpacing(0);
  layout.insertSpacing(0, 0);

  label.setText(text);
  layout.addWidget(&label, 0, Qt::AlignCenter);
  label.setStyleSheet(R"(font-size: 30px; font-weight: 580; color: #FFFF00;)");

  setFixedWidth(240);
  setLayout(&layout);
}

void IPWidget::update(QString text){
  label.setText(text);
}

Sidebar::Sidebar(QWidget* parent) : QFrame(parent) {
  QVBoxLayout* layout = new QVBoxLayout();
  layout->setContentsMargins(25, 50, 25, 50);
  layout->setSpacing(20);
  setFixedSize(300, vwp_h);

  // QPushButton *s_btn = new QPushButton;
  // s_btn->setStyleSheet(R"(
  //   border-image: url(../assets/images/button_settings.png);
  // )");
  // s_btn->setFixedSize(200, 117);
  // layout->addWidget(s_btn, 0, Qt::AlignHCenter);
  // QObject::connect(s_btn, &QPushButton::pressed, this, &Sidebar::openSettings);

  QImage image_setting = QImageReader("../assets/images/button_settings.png").read();
  QLabel *comma_setting = new QLabel(this);
  comma_setting->setPixmap(QPixmap::fromImage(image_setting));
  comma_setting->setAlignment(Qt::AlignCenter);
  layout->addWidget(comma_setting, 1, Qt::AlignHCenter | Qt::AlignVCenter);

  signal = new SignalWidget("--", 0, this);
  layout->addWidget(signal, 0, Qt::AlignTop | Qt::AlignHCenter);

  ipaddr = new IPWidget("N/A", this);
  layout->addWidget(ipaddr, 0, Qt::AlignTop);

  temp = new StatusWidget("0°C", "시스템온도", QColor(255, 255, 255), this);
  layout->addWidget(temp, 0, Qt::AlignTop);

  panda = new StatusWidget("차량\n연결안됨", "", QColor(201, 34, 49), this);
  layout->addWidget(panda, 0, Qt::AlignTop);

  connect = new StatusWidget("인터넷\n오프라인", "",  QColor(218, 202, 37), this);
  layout->addWidget(connect, 0, Qt::AlignTop);

  QImage image = QImageReader("../assets/images/button_home.png").read();
  QLabel *comma = new QLabel(this);
  comma->setPixmap(QPixmap::fromImage(image));
  comma->setAlignment(Qt::AlignCenter);
  layout->addWidget(comma, 1, Qt::AlignHCenter | Qt::AlignVCenter);

  layout->addStretch(1);

  setStyleSheet(R"(
    Sidebar {
      background-color: #393939;
    }
    * {
      color: white;
    }
  )");
  setLayout(layout);
}

void Sidebar::update(const UIState &s){
  static std::map<NetStatus, std::pair<QString, QColor>> connectivity_map = {
    {NET_ERROR, {"인터넷\n연결오류", COLOR_DANGER}},
    {NET_CONNECTED, {"인터넷\n온라인", COLOR_GOOD}},
    {NET_DISCONNECTED, {"인터넷\n오프라인", COLOR_WARNING}},
  };
  auto net_params = connectivity_map[s.scene.athenaStatus];
  connect->update(net_params.first, "", net_params.second);

  static std::map<cereal::DeviceState::ThermalStatus, QColor> temp_severity_map = {
        {cereal::DeviceState::ThermalStatus::GREEN, COLOR_GOOD},
        {cereal::DeviceState::ThermalStatus::YELLOW, COLOR_WARNING},
        {cereal::DeviceState::ThermalStatus::RED, COLOR_DANGER},
        {cereal::DeviceState::ThermalStatus::DANGER, COLOR_DANGER}};
  QString temp_val = QString("%1 °C").arg((int)s.scene.deviceState.getAmbientTempC());
  temp->update(temp_val, "시스템온도", temp_severity_map[s.scene.deviceState.getThermalStatus()]);

  static std::map<cereal::DeviceState::NetworkType, const char *> network_type_map = {
      {cereal::DeviceState::NetworkType::NONE, "--"},
      {cereal::DeviceState::NetworkType::WIFI, "WiFi"},
      {cereal::DeviceState::NetworkType::CELL2_G, "2G"},
      {cereal::DeviceState::NetworkType::CELL3_G, "3G"},
      {cereal::DeviceState::NetworkType::CELL4_G, "4G"},
      {cereal::DeviceState::NetworkType::CELL5_G, "5G"}};
  const char *network_type = network_type_map[s.scene.deviceState.getNetworkType()];
  static std::map<cereal::DeviceState::NetworkStrength, int> network_strength_map = {
      {cereal::DeviceState::NetworkStrength::UNKNOWN, 1},
      {cereal::DeviceState::NetworkStrength::POOR, 2},
      {cereal::DeviceState::NetworkStrength::MODERATE, 3},
      {cereal::DeviceState::NetworkStrength::GOOD, 4},
      {cereal::DeviceState::NetworkStrength::GREAT, 5}};
  const int img_idx = s.scene.deviceState.getNetworkType() == cereal::DeviceState::NetworkType::NONE ? 0 : network_strength_map[s.scene.deviceState.getNetworkStrength()];
  signal->update(network_type, img_idx, s.scene);

  // opkr
  // QString bat_lvl = QString("  %1%").arg((int)s.scene.deviceState.getBatteryPercent());
  // QString bat_stat = "";
  // if (s.scene.deviceState.getBatteryStatus() == "Charging") {
  //   bat_stat = "+";
  // } else {
  //   bat_stat = "-";
  // }
  // signal->update(network_type + bat_lvl + bat_stat, img_idx);
  ipaddr->update(s.scene.ipAddr);

  QColor panda_color = COLOR_GOOD;
  QString panda_message = "차량\n연결됨";
  if (s.scene.pandaType == cereal::PandaState::PandaType::UNKNOWN) {
    panda_color = COLOR_DANGER;
    panda_message = "차량\n연결안됨";
  } else if (s.scene.satelliteCount > 0) {
    panda_message = QString("차량연결됨\nSAT : %1").arg(s.scene.satelliteCount);
  }
#ifdef QCOM2
  else if (s.scene.started) {
    panda_color = s.scene.gpsOK ? COLOR_GOOD : COLOR_WARNING;
    panda_message = QString("SAT CNT\n%1").arg(s.scene.satelliteCount);
  }
#endif
  panda->update(panda_message, "", panda_color);
}