/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef FIFO_QUEUE_ECN_DISC_H
#define FIFO_QUEUE_ECN_DISC_H

#include "queue-disc.h"

namespace ns3
{

/**
 * @ingroup traffic-control
 *
 * Simple queue disc implementing the FIFO (First-In First-Out) policy with
 * ECN marking.
 */
class FifoQueueEcnDisc : public QueueDisc
{
  public:
    /**
     * @brief Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId();

    /**
     * @brief FifoQueueEcnDisc constructor.
     *
     * Creates a queue with a depth of 1000 packets by default.
     */
    FifoQueueEcnDisc();

    ~FifoQueueEcnDisc() override;

    // Reasons for dropping packets
    static constexpr const char* LIMIT_EXCEEDED_DROP =
        "Queue disc limit exceeded"; //!< Packet dropped due to queue disc limit exceeded

    // Reasons for marking packets
    static constexpr const char* ECN_MARK =
        "ECN mark"; //!< Packet marked due to ECN marking threshold exceeded

  private:
    bool DoEnqueue(Ptr<QueueDiscItem> item) override;
    Ptr<QueueDiscItem> DoDequeue() override;
    Ptr<const QueueDiscItem> DoPeek() override;
    bool CheckConfig() override;
    void InitializeParams() override;

    double m_markThreshold; //!< Fraction of MaxSize above which packets are marked
    uint64_t m_accuLen;
    Time m_prevTs;
};

} // namespace ns3

#endif /* FIFO_QUEUE_ECN_DISC_H */
