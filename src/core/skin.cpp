/**
 * 文件名：skin.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：实现平台无关皮肤目录解析、皮肤会话与逻辑资源路径解析。
 */
#include "ysDui/core/DuiSkin.hpp"

#include <algorithm>
#include <utility>

#include "pugixml.hpp"

namespace ysDui::core {
namespace {

std::string JoinResourcePath(std::string_view prefix, std::string_view path)
{
    while (!prefix.empty() && prefix.back() == '/')
        prefix.remove_suffix(1);
    while (!path.empty() && path.front() == '/')
        path.remove_prefix(1);
    if (prefix.empty())
        return std::string(path);
    if (path.empty())
        return std::string(prefix);
    return std::string(prefix) + "/" + std::string(path);
}

std::optional<DuiSkinDescriptor> FindSkin(const std::vector<DuiSkinDescriptor>& skins, int id)
{
    const auto found = std::find_if(skins.begin(), skins.end(), [id](const DuiSkinDescriptor& skin)
    {
        return skin.id == id;
    });
    if (found == skins.end())
        return {};
    return *found;
}

} // namespace

class DuiSkinCatalog::Impl final
{
public:
    std::vector<DuiSkinDescriptor> skins;  // 保持 XML 声明顺序的皮肤列表。
    std::optional<int> configuredDefault;  // CurSkinID 指定的默认皮肤标识。
};

DuiSkinCatalog::DuiSkinCatalog() : catalog_(std::make_unique<Impl>())
{
}

DuiSkinCatalog::~DuiSkinCatalog() = default;
DuiSkinCatalog::DuiSkinCatalog(DuiSkinCatalog&&) noexcept = default;
DuiSkinCatalog& DuiSkinCatalog::operator=(DuiSkinCatalog&&) noexcept = default;

bool DuiSkinCatalog::Load(std::string_view xml)
{
    pugi::xml_document document;
    if (!document.load_buffer(xml.data(), xml.size(), pugi::parse_default, pugi::encoding_utf8))
        return false;
    const pugi::xml_node root = document.child("SkinList");
    if (!root)
        return false;

    std::vector<DuiSkinDescriptor> loaded;
    for (const pugi::xml_node node : root.children("Skin"))
    {
        const pugi::xml_node idNode = node.child("SkinID");
        const pugi::xml_node nameNode = node.child("SkinName");
        const pugi::xml_node pathNode = node.child("SkinPath");
        if (!idNode || !nameNode || !pathNode)
            return false;
        DuiSkinDescriptor skin;
        skin.id = idNode.text().as_int();
        skin.name = nameNode.text().as_string();
        skin.resourcePrefix = pathNode.text().as_string();
        if (skin.name.empty() || FindSkin(loaded, skin.id))
            return false;
        loaded.push_back(std::move(skin));
    }
    if (loaded.empty())
        return false;

    std::optional<int> configuredDefault;
    if (const pugi::xml_node defaultNode = root.child("CurSkinID"))
    {
        const int id = defaultNode.text().as_int();
        if (!FindSkin(loaded, id))
            return false;
        configuredDefault = id;
    }

    catalog_->skins = std::move(loaded);
    catalog_->configuredDefault = configuredDefault;
    return true;
}

std::vector<DuiSkinDescriptor> DuiSkinCatalog::Skins() const
{
    return catalog_->skins;
}

std::optional<DuiSkinDescriptor> DuiSkinCatalog::Find(int id) const
{
    return FindSkin(catalog_->skins, id);
}

std::optional<DuiSkinDescriptor> DuiSkinCatalog::DefaultSkin() const
{
    if (catalog_->configuredDefault)
        return Find(*catalog_->configuredDefault);
    if (catalog_->skins.empty())
        return {};
    return catalog_->skins.front();
}

class DuiSkinSession::Impl final
{
public:
    struct SubscriptionStore final
    {
        struct Subscription final
        {
            std::size_t token{};
            std::function<void()> callback;
        };

        std::vector<Subscription> subscriptions;
        std::size_t nextToken{1};
    };

    std::shared_ptr<const DuiSkinCatalog> catalog;
    std::optional<int> currentSkinId;
    std::shared_ptr<SubscriptionStore> subscriptionStore{std::make_shared<SubscriptionStore>()};
};

void DuiSkinSession::Notify(Impl& session)
{
    const auto subscriptions = session.subscriptionStore->subscriptions;
    for (const auto& subscription : subscriptions)
    {
        if (subscription.callback)
            subscription.callback();
    }
}

DuiSkinSession::DuiSkinSession() : session_(std::make_unique<Impl>())
{
}

DuiSkinSession::~DuiSkinSession() = default;
DuiSkinSession::DuiSkinSession(DuiSkinSession&&) noexcept = default;
DuiSkinSession& DuiSkinSession::operator=(DuiSkinSession&&) noexcept = default;

void DuiSkinSession::SetCatalog(std::shared_ptr<const DuiSkinCatalog> catalog)
{
    const auto defaultSkin = catalog ? catalog->DefaultSkin() : std::optional<DuiSkinDescriptor>{};
    const std::optional<int> nextSkinId = defaultSkin ? std::optional<int>{defaultSkin->id} : std::nullopt;
    const bool changed = session_->catalog != catalog || session_->currentSkinId != nextSkinId;
    session_->catalog = std::move(catalog);
    session_->currentSkinId = nextSkinId;
    if (changed)
        Notify(*session_);
}

bool DuiSkinSession::SetCurrentSkin(int id)
{
    if (!session_->catalog || !session_->catalog->Find(id))
        return false;
    if (session_->currentSkinId == id)
        return true;
    session_->currentSkinId = id;
    Notify(*session_);
    return true;
}

std::optional<DuiSkinDescriptor> DuiSkinSession::CurrentSkin() const
{
    if (!session_->catalog || !session_->currentSkinId)
        return {};
    return session_->catalog->Find(*session_->currentSkinId);
}

std::string DuiSkinSession::ResolveResourcePath(std::string_view path) const
{
    const auto skin = CurrentSkin();
    return skin ? JoinResourcePath(skin->resourcePrefix, path) : std::string(path);
}

DuiSubscription DuiSkinSession::SubscribeScoped(std::function<void()> callback)
{
    if (!callback)
        return {};
    const auto store = session_->subscriptionStore;
    const std::size_t token = store->nextToken++;
    DuiSubscription subscription([weakStore = std::weak_ptr<Impl::SubscriptionStore>{store}, token]
    {
        if (const auto lockedStore = weakStore.lock())
        {
            std::erase_if(lockedStore->subscriptions, [token](const Impl::SubscriptionStore::Subscription& subscription)
            {
                return subscription.token == token;
            });
        }
    });
    store->subscriptions.push_back({token, std::move(callback)});
    return subscription;
}

std::size_t DuiSkinSession::Subscribe(std::function<void()> callback)
{
    if (!callback)
        return 0;
    const std::size_t token = session_->subscriptionStore->nextToken++;
    session_->subscriptionStore->subscriptions.push_back({token, std::move(callback)});
    return token;
}

void DuiSkinSession::Unsubscribe(std::size_t token)
{
    std::erase_if(session_->subscriptionStore->subscriptions,
                  [token](const Impl::SubscriptionStore::Subscription& subscription)
    {
        return subscription.token == token;
    });
}

} // namespace ysDui::core
