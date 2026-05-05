/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "fifo-queue-ecn-disc.h"

#include "ns3/double.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/log.h"
#include "ns3/object-factory.h"
#include "ns3/simulator.h"

#include <algorithm>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("FifoQueueEcnDisc");

NS_OBJECT_ENSURE_REGISTERED(FifoQueueEcnDisc);

TypeId
FifoQueueEcnDisc::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::FifoQueueEcnDisc")
            .SetParent<QueueDisc>()
            .SetGroupName("TrafficControl")
            .AddConstructor<FifoQueueEcnDisc>()
            .AddAttribute("MaxSize",
                          "The max queue size",
                          QueueSizeValue(QueueSize("1000p")),
                          MakeQueueSizeAccessor(&QueueDisc::SetMaxSize, &QueueDisc::GetMaxSize),
                          MakeQueueSizeChecker())
            .AddAttribute("MarkThreshold",
                          "The fraction of MaxSize above which packets are marked",
                          DoubleValue(1.0),
                          MakeDoubleAccessor(&FifoQueueEcnDisc::m_markThreshold),
                          MakeDoubleChecker<double>(0.0, 1.0));
    return tid;
}

FifoQueueEcnDisc::FifoQueueEcnDisc()
    : QueueDisc(QueueDiscSizePolicy::SINGLE_INTERNAL_QUEUE)
{
    NS_LOG_FUNCTION(this);
}

FifoQueueEcnDisc::~FifoQueueEcnDisc()
{
    NS_LOG_FUNCTION(this);
}

bool
FifoQueueEcnDisc::DoEnqueue(Ptr<QueueDiscItem> item)
{
    NS_LOG_FUNCTION(this << item);

    static uint64_t accu_len = 0;
    static Time prev = Seconds(0);
    Time now = Simulator::Now();
    Time interval = now - prev;
    prev = now;
    int qsize = accu_len - interval * 1000;
    accu_len = qsize + item;

    QueueSize newSize = GetCurrentSize() + item;

    if (newSize > GetMaxSize())
    {
        NS_LOG_LOGIC("Queue full -- dropping pkt");
        DropBeforeEnqueue(item, LIMIT_EXCEEDED_DROP);
        return false;
    }

    uint32_t markThreshold =
        static_cast<uint32_t>(m_markThreshold * GetMaxSize().GetValue());
    markThreshold = std::max(markThreshold, 1U);

    if (qsize >= markThreshold*750*1024 && Mark(item, ECN_MARK))
    {
        NS_LOG_LOGIC("Marking packet due to ECN marking threshold");
    }

    bool retval = GetInternalQueue(0)->Enqueue(item);

    // If Queue::Enqueue fails, QueueDisc::DropBeforeEnqueue is called by the
    // internal queue because QueueDisc::AddInternalQueue sets the trace callback

    NS_LOG_LOGIC("Number packets " << GetInternalQueue(0)->GetNPackets());
    NS_LOG_LOGIC("Number bytes " << GetInternalQueue(0)->GetNBytes());

    return retval;
}

Ptr<QueueDiscItem>
FifoQueueEcnDisc::DoDequeue()
{
    NS_LOG_FUNCTION(this);

    Ptr<QueueDiscItem> item = GetInternalQueue(0)->Dequeue();

    if (!item)
    {
        NS_LOG_LOGIC("Queue empty");
        return nullptr;
    }

    return item;
}

Ptr<const QueueDiscItem>
FifoQueueEcnDisc::DoPeek()
{
    NS_LOG_FUNCTION(this);

    Ptr<const QueueDiscItem> item = GetInternalQueue(0)->Peek();

    if (!item)
    {
        NS_LOG_LOGIC("Queue empty");
        return nullptr;
    }

    return item;
}

bool
FifoQueueEcnDisc::CheckConfig()
{
    NS_LOG_FUNCTION(this);
    if (GetNQueueDiscClasses() > 0)
    {
        NS_LOG_ERROR("FifoQueueEcnDisc cannot have classes");
        return false;
    }

    if (GetNPacketFilters() > 0)
    {
        NS_LOG_ERROR("FifoQueueEcnDisc needs no packet filter");
        return false;
    }

    if (GetNInternalQueues() == 0)
    {
        // add a DropTail queue
        AddInternalQueue(
            CreateObjectWithAttributes<DropTailQueue<QueueDiscItem>>("MaxSize",
                                                                     QueueSizeValue(GetMaxSize())));
    }

    if (GetNInternalQueues() != 1)
    {
        NS_LOG_ERROR("FifoQueueEcnDisc needs 1 internal queue");
        return false;
    }

    return true;
}

void
FifoQueueEcnDisc::InitializeParams()
{
    NS_LOG_FUNCTION(this);
}

} // namespace ns3
