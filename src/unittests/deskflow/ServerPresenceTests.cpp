/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServerPresenceTests.h"

#include "MockEventQueue.h"
#include "deskflow/ServerPresence.h"

#include <atomic>

namespace {

class ReadyCheckingEventQueue : public MockEventQueue
{
public:
  void waitForReady() const override
  {
    m_waitedForReady.store(true);
  }

  bool waitedForReady() const
  {
    return m_waitedForReady.load();
  }

private:
  mutable std::atomic_bool m_waitedForReady{false};
};

} // namespace

void ServerPresenceTests::validAnnouncement()
{
  const auto announcement = deskflow::ServerPresence::announcement(QStringLiteral("Ally"), 24800);

  QCOMPARE(announcement, QByteArray("DESKFLOW_SERVER_READY_V1\nAlly\n24800"));
  QVERIFY(deskflow::ServerPresence::isAnnouncement(announcement));
}

void ServerPresenceTests::invalidAnnouncement()
{
  QVERIFY(!deskflow::ServerPresence::isAnnouncement(QByteArray{}));
  QVERIFY(!deskflow::ServerPresence::isAnnouncement("DESKFLOW_SERVER_READY_V1\n\n24800"));
  QVERIFY(!deskflow::ServerPresence::isAnnouncement("DESKFLOW_SERVER_READY_V1\nAlly\n0"));
  QVERIFY(!deskflow::ServerPresence::isAnnouncement("DESKFLOW_SERVER_READY_V1\nAlly\ninvalid"));
  QVERIFY(!deskflow::ServerPresence::isAnnouncement("SOME_OTHER_SERVICE\nAlly\n24800"));
  QVERIFY(!deskflow::ServerPresence::isAnnouncement("DESKFLOW_SERVER_READY_V1\nAlly\n65536"));
  QVERIFY(!deskflow::ServerPresence::isAnnouncement("DESKFLOW_SERVER_READY_V1\nAlly\n24800\nextra"));
}

void ServerPresenceTests::listenerDoesNotWaitForEventQueue()
{
  ReadyCheckingEventQueue events;
  deskflow::ServerPresenceListener listener(&events, this);

  listener.start();
  listener.stop();

  QVERIFY(!events.waitedForReady());
}

QTEST_MAIN(ServerPresenceTests)
