#pragma once

#include <QtWidgets>

#include <ui.hpp>

#define COLOR_GOOD QColor(255, 255, 255)
#define COLOR_WARNING QColor(218, 202, 37)
#define COLOR_DANGER QColor(201, 34, 49)

class SignalWidget : public QFrame {
  Q_OBJECT

public:
  SignalWidget(QString text, int strength, QWidget* parent = 0);
  void update(QString text, int strength, const UIScene &scene);
  QLabel label;
  int _strength = 0;

  QLabel label_ip;

  QImage image_bty;
  int    m_batteryPercent;
  int    m_battery_img;

protected:
  void paintEvent(QPaintEvent*) override;

private:
  QVBoxLayout layout;

  const float _dotspace = 37; // spacing between dots
  const float _top = 10;
  const float _dia = 28; // dot diameter
};

class IPWidget : public QFrame {
  Q_OBJECT

public:
  IPWidget(QString text, QWidget* parent = 0);
  void update(QString text);
  QLabel label;

private:
  QVBoxLayout layout;
};

class StatusWidget : public QFrame {
  Q_OBJECT

public:
  StatusWidget(QString label, QString msg, QColor c, QWidget* parent = 0);
  void update(QString label, QString msg, QColor c);

protected:
  void paintEvent(QPaintEvent*) override;

private:
  QColor color = COLOR_WARNING;
  QLabel status;
  QLabel substatus;
  QVBoxLayout layout;
};

class Sidebar : public QFrame {
  Q_OBJECT

public:
  explicit Sidebar(QWidget* parent = 0);

signals:
  void openSettings();

public slots:
  void update(const UIState &s);

private:
  SignalWidget *signal;
  IPWidget *ipaddr;
  StatusWidget *temp;
  StatusWidget *panda;
  StatusWidget *connect;
};
