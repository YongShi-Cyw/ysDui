/**
 * 文件名：DuiAsyncImageLoader.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：实现平台无关异步图像请求的合并、缓存、取消和结果投递。
 */
#include "ysDui/controls/media/DuiAsyncImageLoader.hpp"

#include <algorithm>
#include <condition_variable>
#include <list>
#include <mutex>
#include <ranges>
#include <thread>
#include <unordered_map>
#include <utility>

namespace ysDui::controls::media {
namespace {

struct Waiter final
{
    std::uint64_t requestId{};
    std::uint64_t userToken{};
    bool cancelled{};
};

struct InFlight final
{
    std::vector<Waiter> waiters;
};

struct CacheEntry final
{
    std::shared_ptr<const render::DuiImage> image;
    std::list<std::string>::iterator position;
};

} // namespace

class DuiAsyncImageLoader::Impl
{
public:
    Impl() : worker(&Impl::run, this) {}

    ~Impl()
    {
        {
            std::lock_guard lock(mutex);
            stopping = true;
        }
        wake.notify_one();
        worker.join();
    }

    void run()
    {
        for (;;) {
            std::string path;
            Decoder decode;
            {
                std::unique_lock lock(mutex);
                wake.wait(lock, [this] { return stopping || !pending.empty(); });
                if (stopping)
                    return;
                path = std::move(pending.front());
                pending.pop_front();
                const auto inFlightEntry = inFlight.find(path);
                if (inFlightEntry == inFlight.end() || allCancelled(inFlightEntry->second))
                    continue;
                decode = decoder;
            }

            std::shared_ptr<const render::DuiImage> decoded;
            if (decode)
                decoded = decode(path);

            std::lock_guard lock(mutex);
            const auto inFlightEntry = inFlight.find(path);
            if (inFlightEntry == inFlight.end())
                continue;
            std::shared_ptr<const render::DuiImage> image = std::move(decoded);
            if (image) {
                cacheImage(path, image);
            }
            for (const Waiter& waiter : inFlightEntry->second.waiters) {
                if (waiter.cancelled)
                    continue;
                completed.push_back({waiter.requestId, waiter.userToken, image,
                                     image ? DuiAsyncImageError::Succeeded : DuiAsyncImageError::DecodeFailed});
            }
            inFlight.erase(inFlightEntry);
        }
    }

    static bool allCancelled(const InFlight& value)
    {
        return std::ranges::all_of(value.waiters, [](const Waiter& waiter) { return waiter.cancelled; });
    }

    void cacheImage(const std::string& path, const std::shared_ptr<const render::DuiImage>& image)
    {
        if (cacheCapacity == 0)
            return;
        const auto existing = cache.find(path);
        if (existing != cache.end()) {
            lru.erase(existing->second.position);
            cache.erase(existing);
        }
        lru.push_front(path);
        cache.emplace(path, CacheEntry{image, lru.begin()});
        while (cache.size() > cacheCapacity) {
            const std::string& victim = lru.back();
            cache.erase(victim);
            lru.pop_back();
        }
    }

    std::mutex mutex;
    std::condition_variable wake;
    std::thread worker;
    Decoder decoder;
    std::uint64_t nextRequestId{1};
    bool stopping{};
    std::size_t cacheCapacity{128};
    std::list<std::string> pending;
    std::list<std::string> lru;
    std::unordered_map<std::string, InFlight> inFlight;
    std::unordered_map<std::string, CacheEntry> cache;
    std::vector<DuiAsyncImageResult> completed;
};

DuiAsyncImageLoader::DuiAsyncImageLoader() : loader_(std::make_unique<Impl>()) {}
DuiAsyncImageLoader::~DuiAsyncImageLoader() = default;

void DuiAsyncImageLoader::SetDecoder(Decoder decoder)
{
    std::lock_guard lock(loader_->mutex);
    loader_->decoder = std::move(decoder);
}

std::uint64_t DuiAsyncImageLoader::Submit(std::string path, std::uint64_t userToken)
{
    if (path.empty())
        return 0;
    std::lock_guard lock(loader_->mutex);
    if (!loader_->decoder)
        return 0;
    const std::uint64_t requestId = loader_->nextRequestId++;
    if (const auto cached = loader_->cache.find(path); cached != loader_->cache.end()) {
        loader_->lru.splice(loader_->lru.begin(), loader_->lru, cached->second.position);
        cached->second.position = loader_->lru.begin();
        loader_->completed.push_back({requestId, userToken, cached->second.image, DuiAsyncImageError::Succeeded});
        return requestId;
    }
    const auto [entry, inserted] = loader_->inFlight.try_emplace(path);
    entry->second.waiters.push_back({requestId, userToken, false});
    if (inserted) {
        loader_->pending.push_back(path);
        loader_->wake.notify_one();
    }
    return requestId;
}

void DuiAsyncImageLoader::Cancel(std::uint64_t requestId)
{
    if (requestId == 0)
        return;
    std::lock_guard lock(loader_->mutex);
    for (auto& [_, request] : loader_->inFlight) {
        for (Waiter& waiter : request.waiters) {
            if (waiter.requestId == requestId && !waiter.cancelled) {
                waiter.cancelled = true;
                loader_->completed.push_back({waiter.requestId, waiter.userToken, {},
                                              DuiAsyncImageError::Cancelled});
            }
        }
    }
}

std::vector<DuiAsyncImageResult> DuiAsyncImageLoader::Poll()
{
    std::lock_guard lock(loader_->mutex);
    return std::exchange(loader_->completed, {});
}

void DuiAsyncImageLoader::SetCacheCapacity(std::size_t capacity)
{
    std::lock_guard lock(loader_->mutex);
    loader_->cacheCapacity = capacity;
    while (loader_->cache.size() > capacity) {
        const std::string& victim = loader_->lru.back();
        loader_->cache.erase(victim);
        loader_->lru.pop_back();
    }
}

void DuiAsyncImageLoader::ClearCache()
{
    std::lock_guard lock(loader_->mutex);
    loader_->cache.clear();
    loader_->lru.clear();
}

} // namespace ysDui::controls::media
