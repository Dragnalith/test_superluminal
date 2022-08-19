#pragma once

#include <assert.h>
#include <memory>

namespace app
{

#define ASSERT_MSG(condition, msg) assert(msg && condition)

template <typename T>
class Pimpl
{
public:
    template <typename... Args>
    Pimpl(Args&&... args) : m_content(std::make_unique<T>(std::forward<Args>(args)...))
    {
    }

    Pimpl(Pimpl&& other)
        : m_content(std::move(other.m_impl))
    {}

    T* operator ->() {
        return m_content.get();
    }

    const T* operator->() const{
        return m_content.get();
    }

    T* get() {
        return m_content.get();
    }

    const T* get() const {
        return m_content.get();
    }

private:
    std::unique_ptr<T> m_content;
};

}