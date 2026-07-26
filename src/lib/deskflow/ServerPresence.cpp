/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/ServerPresence.h"

#include "base/Event.h"
#include "base/IEventQueue.h"
#include "base/Log.h"

#include <QAbstractSocket>
#include <QHostAddress>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QSet>
#include <QUdpSocket>

namespace deskflow {

namespace {
constexpr auto kAnnouncementMagic = "DESKFLOW_SERVER_READY_V1";
}

QByteArray ServerPresence::announcement(const QString &serverName, quint16 servicePort)
{
  return QByteArray(kAnnouncementMagic) + '\n' + serverName.toUtf8() + '\n' + QByteArray::number(servicePort);
}

bool ServerPresence::isAnnouncement(const QByteArray &datagram)
{
  const auto fields = datagram.split('\n');
  if (fields.size() != 3 || fields.at(0) != kAnnouncementMagic || fields.at(1).isEmpty()) {
    return false;
  }

  bool validPort = false;
  const auto port = fields.at(2).toUShort(&validPort);
  return validPort && port > 0;
}

void ServerPresence::announce(const QString &serverName, quint16 servicePort)
{
  const auto datagram = announcement(serverName, servicePort);
  QSet<QHostAddress> destinations;
  destinations.insert(QHostAddress(QHostAddress::Broadcast));

  for (const auto &interface : QNetworkInterface::allInterfaces()) {
    const auto flags = interface.flags();
    if (!flags.testFlag(QNetworkInterface::IsUp) || !flags.testFlag(QNetworkInterface::IsRunning) ||
        flags.testFlag(QNetworkInterface::IsLoopBack)) {
      continue;
    }

    for (const auto &entry : interface.addressEntries()) {
      if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol || entry.ip().isLinkLocal()) {
        continue;
      }

      const auto broadcast = entry.broadcast();
      if (!broadcast.isNull()) {
        destinations.insert(broadcast);
      }
    }
  }

  QUdpSocket socket;
  int sent = 0;
  for (const auto &destination : destinations) {
    if (socket.writeDatagram(datagram, destination, kDiscoveryPort) == datagram.size()) {
      sent++;
    }
  }

  LOG_DEBUG("sent server presence announcement to %d network(s)", sent);
}

ServerPresenceListener::ServerPresenceListener(IEventQueue *events, void *eventTarget)
    : m_events(events),
      m_eventTarget(eventTarget)
{
}

ServerPresenceListener::~ServerPresenceListener()
{
  stop();
}

void ServerPresenceListener::start()
{
  if (m_thread.joinable()) {
    return;
  }

  m_stopping.store(false);
  m_thread = std::thread(&ServerPresenceListener::run, this);
}

void ServerPresenceListener::stop()
{
  if (!m_thread.joinable()) {
    return;
  }

  m_stopping.store(true);

  // Wake waitForReadyRead so shutdown does not need to wait for its timeout.
  QUdpSocket wakeSocket;
  wakeSocket.writeDatagram(QByteArray{}, QHostAddress(QHostAddress::LocalHost), ServerPresence::kDiscoveryPort);
  m_thread.join();
}

void ServerPresenceListener::run()
{
  QUdpSocket socket;
  const auto bindMode = QAbstractSocket::ShareAddress | QAbstractSocket::ReuseAddressHint;
  if (!socket.bind(QHostAddress::AnyIPv4, ServerPresence::kDiscoveryPort, bindMode)) {
    LOG_WARN("failed to listen for server presence announcements: %s", qPrintable(socket.errorString()));
    return;
  }

  LOG_DEBUG(
      "listening for server presence announcements on UDP port %u",
      static_cast<unsigned>(ServerPresence::kDiscoveryPort)
  );
  while (!m_stopping.load()) {
    if (!socket.waitForReadyRead(500)) {
      continue;
    }

    while (socket.hasPendingDatagrams()) {
      const auto datagram = socket.receiveDatagram();
      if (!ServerPresence::isAnnouncement(datagram.data())) {
        continue;
      }

      LOG_DEBUG("received server presence announcement from %s", qPrintable(datagram.senderAddress().toString()));
      m_events->addEvent(Event(EventTypes::ClientServerPresence, m_eventTarget));
    }
  }
}

} // namespace deskflow
