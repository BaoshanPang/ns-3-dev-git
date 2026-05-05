/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ns3/double.h"
#include "ns3/fifo-queue-ecn-disc.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/test.h"

#include <vector>

using namespace ns3;

/**
 * @ingroup traffic-control-test
 *
 * @brief Fifo Queue ECN Disc Test Item
 */
class FifoQueueEcnDiscTestItem : public QueueDiscItem
{
  public:
    /**
     * Constructor.
     *
     * @param p the packet
     * @param addr the address
     * @param ecnCapable whether the item can be ECN marked
     */
    FifoQueueEcnDiscTestItem(Ptr<Packet> p, const Address& addr, bool ecnCapable);
    ~FifoQueueEcnDiscTestItem() override;

    // Delete default constructor, copy constructor and assignment operator to avoid misuse
    FifoQueueEcnDiscTestItem() = delete;
    FifoQueueEcnDiscTestItem(const FifoQueueEcnDiscTestItem&) = delete;
    FifoQueueEcnDiscTestItem& operator=(const FifoQueueEcnDiscTestItem&) = delete;

    void AddHeader() override;
    bool Mark() override;

    /**
     * @brief Check whether the item was marked.
     * @return true if the item was marked, false otherwise
     */
    bool IsMarked() const;

  private:
    bool m_ecnCapable; //!< Whether the item can be ECN marked
    bool m_marked;     //!< Whether the item has been marked
};

FifoQueueEcnDiscTestItem::FifoQueueEcnDiscTestItem(Ptr<Packet> p,
                                                   const Address& addr,
                                                   bool ecnCapable)
    : QueueDiscItem(p, addr, 0),
      m_ecnCapable(ecnCapable),
      m_marked(false)
{
}

FifoQueueEcnDiscTestItem::~FifoQueueEcnDiscTestItem()
{
}

void
FifoQueueEcnDiscTestItem::AddHeader()
{
}

bool
FifoQueueEcnDiscTestItem::Mark()
{
    if (!m_ecnCapable)
    {
        return false;
    }
    m_marked = true;
    return true;
}

bool
FifoQueueEcnDiscTestItem::IsMarked() const
{
    return m_marked;
}

/**
 * @ingroup traffic-control-test
 *
 * @brief Fifo Queue ECN Disc Test Case
 */
class FifoQueueEcnDiscTestCase : public TestCase
{
  public:
    FifoQueueEcnDiscTestCase();
    void DoRun() override;
};

FifoQueueEcnDiscTestCase::FifoQueueEcnDiscTestCase()
    : TestCase("Sanity check on the fifo queue ECN disc implementation")
{
}

void
FifoQueueEcnDiscTestCase::DoRun()
{
    Ptr<FifoQueueEcnDisc> queue = CreateObject<FifoQueueEcnDisc>();
    Address dest;

    NS_TEST_ASSERT_MSG_EQ(queue->SetAttributeFailSafe(
                              "MaxSize",
                              QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, 4))),
                          true,
                          "Verify that we can set the MaxSize attribute");
    NS_TEST_ASSERT_MSG_EQ(queue->SetAttributeFailSafe("MarkThreshold", DoubleValue(0.5)),
                          true,
                          "Verify that we can set the MarkThreshold attribute");

    queue->Initialize();

    std::vector<Ptr<FifoQueueEcnDiscTestItem>> items;
    std::vector<uint64_t> uids;

    for (uint32_t i = 0; i < 4; i++)
    {
        Ptr<Packet> p = Create<Packet>(100);
        bool ecnCapable = (i != 2);
        Ptr<FifoQueueEcnDiscTestItem> item =
            Create<FifoQueueEcnDiscTestItem>(p, dest, ecnCapable);
        items.push_back(item);
        uids.push_back(p->GetUid());
        NS_TEST_ASSERT_MSG_EQ(queue->Enqueue(item), true, "Packet should be enqueued");
    }

    NS_TEST_EXPECT_MSG_EQ(items[0]->IsMarked(), false, "First packet should not be marked");
    NS_TEST_EXPECT_MSG_EQ(items[1]->IsMarked(), true, "Second packet should be marked");
    NS_TEST_EXPECT_MSG_EQ(items[2]->IsMarked(),
                          false,
                          "Non-ECN capable packet should not be marked");
    NS_TEST_EXPECT_MSG_EQ(items[3]->IsMarked(), true, "Fourth packet should be marked");
    NS_TEST_EXPECT_MSG_EQ(queue->GetStats().GetNMarkedPackets(FifoQueueEcnDisc::ECN_MARK),
                          2,
                          "There should be two marked packets");

    Ptr<Packet> p = Create<Packet>(100);
    NS_TEST_EXPECT_MSG_EQ(
        queue->Enqueue(Create<FifoQueueEcnDiscTestItem>(p, dest, true)),
        false,
        "There should be no room for another packet");
    NS_TEST_EXPECT_MSG_EQ(
        queue->GetStats().GetNDroppedPackets(FifoQueueEcnDisc::LIMIT_EXCEEDED_DROP),
        1,
        "There should be one limit exceeded drop");

    for (uint32_t i = 0; i < 4; i++)
    {
        Ptr<QueueDiscItem> item = queue->Dequeue();
        NS_TEST_ASSERT_MSG_NE(item, nullptr, "A packet should have been dequeued");
        NS_TEST_EXPECT_MSG_EQ(item->GetPacket()->GetUid(), uids[i], "Packets should be FIFO");
    }

    NS_TEST_EXPECT_MSG_EQ(queue->Dequeue(), nullptr, "There should be no packets left");

    Simulator::Destroy();
}

/**
 * @ingroup traffic-control-test
 *
 * @brief Fifo Queue ECN Disc Test Suite
 */
static class FifoQueueEcnDiscTestSuite : public TestSuite
{
  public:
    FifoQueueEcnDiscTestSuite()
        : TestSuite("fifo-queue-ecn-disc", Type::UNIT)
    {
        AddTestCase(new FifoQueueEcnDiscTestCase(), TestCase::Duration::QUICK);
    }
} g_fifoQueueEcnTestSuite; ///< the test suite
