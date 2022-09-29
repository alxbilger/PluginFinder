#pragma once

#include <sofa/helper/logging/MessageHandler.h>

class ErrorCountingMessageHandler : public sofa::helper::logging::MessageHandler
{
public:
    ErrorCountingMessageHandler()
    {
        sofa::helper::logging::MessageDispatcher::addHandler(this);
    }

    ~ErrorCountingMessageHandler() override
    {
        sofa::helper::logging::MessageDispatcher::rmHandler(this);
    }

    void reset()
    {
        m_count.clear();
    }

    void process(sofa::helper::logging::Message& m) override
    {
        m_count[m.type()]++;
    }

private:
    std::map<sofa::helper::logging::Message::Type, size_t> m_count;

public:
    [[nodiscard]] size_t getCount(sofa::helper::logging::Message::Type type)
    {
        return m_count[type];
    }
} ;
