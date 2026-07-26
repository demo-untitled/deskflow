/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QByteArray>
#include <QString>
#include <QtTypes>

#include <atomic>
#include <thread>

class IEventQueue;

namespace deskflow {

class ServerPresence
{
public:
  static constexpr quint16 kDiscoveryPort = 24801;

  static QByteArray announcement(const QString &serverName, quint16 servicePort);
  static bool isAnnouncement(const QByteArray &datagram);
  static void announce(const QString &serverName, quint16 servicePort);
};

class ServerPresenceListener
{
public:
  ServerPresenceListener(IEventQueue *events, void *eventTarget);
  ServerPresenceListener(const ServerPresenceListener &) = delete;
  ServerPresenceListener(ServerPresenceListener &&) = delete;
  ~ServerPresenceListener();

  ServerPresenceListener &operator=(const ServerPresenceListener &) = delete;
  ServerPresenceListener &operator=(ServerPresenceListener &&) = delete;

  void start();
  void stop();

private:
  void run();

  IEventQueue *m_events;
  void *m_eventTarget;
  std::atomic_bool m_stopping{false};
  std::thread m_thread;
};

} // namespace deskflow
