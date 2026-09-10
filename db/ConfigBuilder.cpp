#include "db/ConfigBuilder.hpp"
#include "db/Database.hpp"
#include "fmt/includes.h"
#include "fmt/Preset.hpp"

#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QSet>

#define BOX_UNDERLYING_DNS dataStore->core_box_underlying_dns.isEmpty() ? "local" : dataStore->core_box_underlying_dns

namespace NekoGui {

    namespace {
        bool ruleHasMatchersLeft(const QJsonObject &rule) {
            auto copy = rule;
            for (const auto &k: {QStringLiteral("outbound"), QStringLiteral("action"), QStringLiteral("server"),
                                 QStringLiteral("disable_cache"), QStringLiteral("rewrite_ttl"),
                                 QStringLiteral("client_subnet"), QStringLiteral("strategy")}) {
                copy.remove(k);
            }
            return !copy.isEmpty();
        }

        bool stripDeprecatedKeysFromRules(QJsonArray &rules) {
            bool changed = false;
            QJsonArray kept;
            for (const auto &v: rules) {
                if (!v.isObject()) {
                    kept += v;
                    continue;
                }
                auto rule = v.toObject();
                for (const auto &k: {QStringLiteral("geoip"), QStringLiteral("geosite"), QStringLiteral("source_geoip")}) {
                    if (rule.contains(k)) {
                        rule.remove(k);
                        changed = true;
                    }
                }
                if (!ruleHasMatchersLeft(rule)) {
                    changed = true;
                    continue;
                }
                kept += rule;
            }
            if (changed) rules = kept;
            return changed;
        }
    } // namespace

    bool IsDeprecatedGeoError(const QString &error) {
        auto e = error.toLower();
        return e.contains("geoip database is deprecated") || e.contains("geosite database is deprecated") ||
               (e.contains("geoip") && e.contains("removed in sing-box")) ||
               (e.contains("geosite") && e.contains("removed in sing-box"));
    }

    bool StripDeprecatedGeoBlocks(QJsonObject &root) {
        bool changed = false;

        auto stripRouteLike = [&](QJsonObject &obj) {
            if (obj.contains("geoip")) {
                obj.remove("geoip");
                changed = true;
            }
            if (obj.contains("geosite")) {
                obj.remove("geosite");
                changed = true;
            }
            if (obj.contains("rules") && obj["rules"].isArray()) {
                auto rules = obj["rules"].toArray();
                if (stripDeprecatedKeysFromRules(rules)) {
                    obj["rules"] = rules;
                    changed = true;
                }
            }
        };

        if (root.contains("route") && root["route"].isObject()) {
            auto route = root["route"].toObject();
            stripRouteLike(route);
            root["route"] = route;
        }
        if (root.contains("dns") && root["dns"].isObject()) {
            auto dns = root["dns"].toObject();
            if (dns.contains("rules") && dns["rules"].isArray()) {
                auto rules = dns["rules"].toArray();
                if (stripDeprecatedKeysFromRules(rules)) {
                    dns["rules"] = rules;
                    changed = true;
                }
            }
            root["dns"] = dns;
        }
        // Custom Route JSON shape: { "rules": [ ... ] }
        if (root.contains("rules") && root["rules"].isArray()) {
            auto rules = root["rules"].toArray();
            if (stripDeprecatedKeysFromRules(rules)) {
                root["rules"] = rules;
                changed = true;
            }
        }
        return changed;
    }

    bool StripDeprecatedGeoFromJsonString(QString &json) {
        if (json.trimmed().isEmpty()) return false;
        auto obj = QString2QJsonObject(json);
        if (obj.isEmpty() && !json.trimmed().startsWith("{")) return false;
        if (!StripDeprecatedGeoBlocks(obj)) return false;
        json = QJsonObject2QString(obj, false);
        return true;
    }

    bool FixDeprecatedGeoInProfile(const std::shared_ptr<ProxyEntity> &ent) {
        if (ent == nullptr || ent->bean == nullptr) return false;
        bool changed = false;

        auto customBean = dynamic_cast<NekoGui_fmt::CustomBean *>(ent->bean.get());
        if (customBean != nullptr && customBean->core == "internal-full") {
            if (StripDeprecatedGeoFromJsonString(customBean->config_simple)) changed = true;
        }
        if (StripDeprecatedGeoFromJsonString(ent->bean->custom_config)) changed = true;

        if (ScrubDeprecatedGeoFromRoutingStore()) changed = true;

        if (changed) ent->Save();
        return changed;
    }

    bool ScrubDeprecatedGeoFromRoutingStore() {
        bool changed = false;
        bool routingChanged = false;
        if (dataStore->routing != nullptr) {
            if (StripDeprecatedGeoFromJsonString(dataStore->routing->custom)) {
                changed = true;
                routingChanged = true;
            }
            if (dataStore->routing->use_dns_object && StripDeprecatedGeoFromJsonString(dataStore->routing->dns_object)) {
                changed = true;
                routingChanged = true;
            }
            if (routingChanged) dataStore->routing->Save();
        }
        if (StripDeprecatedGeoFromJsonString(dataStore->custom_route_global)) {
            changed = true;
            dataStore->Save();
        }
        return changed;
    }

    QStringList getAutoBypassExternalProcessPaths(const std::shared_ptr<BuildConfigResult> &result) {
        QStringList paths;
        for (const auto &extR: result->extRs) {
            auto path = extR->program;
            if (path.trimmed().isEmpty()) continue;
            paths << path.replace("\\", "/");
        }
        return paths;
    }

    QString genTunName() {
        auto tun_name = "neko-tun";
#ifdef Q_OS_MACOS
        tun_name = "utun9";
#endif
        return tun_name;
    }

    void MergeJson(QJsonObject &dst, const QJsonObject &src) {
        // 合并
        if (src.isEmpty()) return;
        for (const auto &key: src.keys()) {
            auto v_src = src[key];
            if (dst.contains(key)) {
                auto v_dst = dst[key];
                if (v_src.isObject() && v_dst.isObject()) { // isObject 则合并？
                    auto v_src_obj = v_src.toObject();
                    auto v_dst_obj = v_dst.toObject();
                    MergeJson(v_dst_obj, v_src_obj);
                    dst[key] = v_dst_obj;
                } else {
                    dst[key] = v_src;
                }
            } else if (v_src.isArray()) {
                if (key.startsWith("+")) {
                    auto key2 = SubStrAfter(key, "+");
                    auto v_dst = dst[key2];
                    auto v_src_arr = v_src.toArray();
                    auto v_dst_arr = v_dst.toArray();
                    QJSONARRAY_ADD(v_src_arr, v_dst_arr)
                    dst[key2] = v_src_arr;
                } else if (key.endsWith("+")) {
                    auto key2 = SubStrBefore(key, "+");
                    auto v_dst = dst[key2];
                    auto v_src_arr = v_src.toArray();
                    auto v_dst_arr = v_dst.toArray();
                    QJSONARRAY_ADD(v_dst_arr, v_src_arr)
                    dst[key2] = v_dst_arr;
                } else {
                    dst[key] = v_src;
                }
            } else {
                dst[key] = v_src;
            }
        }
    }

    // Common

    std::shared_ptr<BuildConfigResult> BuildConfig(const std::shared_ptr<ProxyEntity> &ent, bool forTest, bool forExport) {
        // Quietly cut deprecated geoip/geosite out of saved Custom Route JSON
        if (!forTest) ScrubDeprecatedGeoFromRoutingStore();

        auto result = std::make_shared<BuildConfigResult>();
        auto status = std::make_shared<BuildConfigStatus>();
        status->ent = ent;
        status->result = result;
        status->forTest = forTest;
        status->forExport = forExport;

        auto customBean = dynamic_cast<NekoGui_fmt::CustomBean *>(ent->bean.get());
        if (customBean != nullptr && customBean->core == "internal-full") {
            result->coreConfig = QString2QJsonObject(customBean->config_simple);
            // Quietly strip deprecated geo from full custom configs (persist if changed)
            if (!forTest && StripDeprecatedGeoBlocks(result->coreConfig)) {
                customBean->config_simple = QJsonObject2QString(result->coreConfig, false);
                ent->Save();
            }
        } else {
            BuildConfigSingBox(status);
        }

        // apply custom config
        auto customCfg = QString2QJsonObject(ent->bean->custom_config);
        if (!forTest && StripDeprecatedGeoBlocks(customCfg)) {
            ent->bean->custom_config = QJsonObject2QString(customCfg, false);
            ent->Save();
        }
        MergeJson(result->coreConfig, customCfg);
        // Final safety: never send geoip/geosite matchers to core
        StripDeprecatedGeoBlocks(result->coreConfig);

        return result;
    }

    QString ProfileOutboundTag(int profileId) {
        return QStringLiteral("p-%1").arg(profileId);
    }

    QString BuildChain(int chainId, const std::shared_ptr<BuildConfigStatus> &status) {
        auto group = profileManager->GetGroup(status->ent->gid);
        if (group == nullptr) {
            status->result->error = QStringLiteral("This profile is not in any group, your data may be corrupted.");
            return {};
        }

        auto resolveChain = [=](const std::shared_ptr<ProxyEntity> &ent) {
            QList<std::shared_ptr<ProxyEntity>> resolved;
            if (ent->type == "chain") {
                auto list = ent->ChainBean()->list;
                std::reverse(std::begin(list), std::end(list));
                for (auto id: list) {
                    resolved += profileManager->GetProfile(id);
                    if (resolved.last() == nullptr) {
                        status->result->error = QStringLiteral("chain missing ent: %1").arg(id);
                        break;
                    }
                    if (resolved.last()->type == "chain") {
                        status->result->error = QStringLiteral("chain in chain is not allowed: %1").arg(id);
                        break;
                    }
                }
            } else {
                resolved += ent;
            };
            return resolved;
        };

        // Make list
        auto ents = resolveChain(status->ent);
        if (!status->result->error.isEmpty()) return {};

        if (group->front_proxy_id >= 0) {
            auto fEnt = profileManager->GetProfile(group->front_proxy_id);
            if (fEnt == nullptr) {
                status->result->error = QStringLiteral("front proxy ent not found.");
                return {};
            }
            ents += resolveChain(fEnt);
            if (!status->result->error.isEmpty()) return {};
        }

        // BuildChain
        QString chainTagOut = BuildChainInternal(0, ents, status);

        // Chain ent traffic stat
        if (ents.length() > 1) {
            status->ent->traffic_data->id = status->ent->id;
            status->ent->traffic_data->tag = chainTagOut.toStdString();
            status->result->outboundStats += status->ent->traffic_data;
        }

        return chainTagOut;
    }

    // Resolve a profile (and nested chain list) without touching status->error on soft failures.
    static QList<std::shared_ptr<ProxyEntity>> ResolveProfileChain(const std::shared_ptr<ProxyEntity> &ent, QString *error) {
        QList<std::shared_ptr<ProxyEntity>> resolved;
        if (!ent) {
            if (error) *error = QStringLiteral("null profile");
            return resolved;
        }
        if (ent->type == "chain") {
            auto list = ent->ChainBean()->list;
            std::reverse(std::begin(list), std::end(list));
            for (auto id: list) {
                resolved += profileManager->GetProfile(id);
                if (resolved.last() == nullptr) {
                    if (error) *error = QStringLiteral("chain missing ent: %1").arg(id);
                    resolved.clear();
                    break;
                }
                if (resolved.last()->type == "chain") {
                    if (error) *error = QStringLiteral("chain in chain is not allowed: %1").arg(id);
                    resolved.clear();
                    break;
                }
            }
        } else {
            resolved += ent;
        }
        return resolved;
    }

    // Embed every other profile in the group as p-<id> so Custom Route can target them.
    static void BuildSplitRoutingOutbounds(const std::shared_ptr<BuildConfigStatus> &status) {
        if (status->forTest || status->forExport) return;
        auto group = profileManager->GetGroup(status->ent->gid);
        if (!group) return;

        for (const auto &pf: group->ProfilesWithOrder()) {
            if (!pf || pf->id == status->ent->id) continue;

            QString resolveErr;
            auto ents = ResolveProfileChain(pf, &resolveErr);
            if (ents.isEmpty()) continue;

            const int outboundsBefore = status->outbounds.size();
            const int rulesBefore = status->routingRules.size();
            const int dnsDirectBefore = status->domainListDNSDirect.size();
            const int ignoreBefore = status->result->ignoreConnTag.size();
            const auto extRsBefore = status->result->extRs.size();
            const int statsBefore = status->result->outboundStats.size();
            const QString errorBefore = status->result->error;

            const QString tag = ProfileOutboundTag(pf->id);
            // chainId != 0 so we never steal the primary "proxy" tag path
            BuildChainInternal(pf->id + 1, ents, status, tag);

            if (!status->result->error.isEmpty() && status->result->error != errorBefore) {
                // Roll back a failed extra outbound; keep primary config working
                while (status->outbounds.size() > outboundsBefore) status->outbounds.removeLast();
                while (status->routingRules.size() > rulesBefore) status->routingRules.removeLast();
                while (status->domainListDNSDirect.size() > dnsDirectBefore) status->domainListDNSDirect.removeLast();
                while (status->result->ignoreConnTag.size() > ignoreBefore) status->result->ignoreConnTag.removeLast();
                while (status->result->outboundStats.size() > statsBefore) status->result->outboundStats.removeLast();
                while (status->result->extRs.size() > extRsBefore) status->result->extRs.pop_back();
                status->result->error = errorBefore;
            }
        }
    }

#define DOMAIN_USER_RULE                                                             \
    for (const auto &line: SplitLinesSkipSharp(dataStore->routing->proxy_domain)) {  \
        if (dataStore->routing->dns_routing) status->domainListDNSRemote += line;    \
        status->domainListRemote += line;                                            \
    }                                                                                \
    for (const auto &line: SplitLinesSkipSharp(dataStore->routing->direct_domain)) { \
        if (dataStore->routing->dns_routing) status->domainListDNSDirect += line;    \
        status->domainListDirect += line;                                            \
    }                                                                                \
    for (const auto &line: SplitLinesSkipSharp(dataStore->routing->block_domain)) {  \
        status->domainListBlock += line;                                             \
    }

#define IP_USER_RULE                                                             \
    for (const auto &line: SplitLinesSkipSharp(dataStore->routing->block_ip)) {  \
        status->ipListBlock += line;                                             \
    }                                                                            \
    for (const auto &line: SplitLinesSkipSharp(dataStore->routing->proxy_ip)) {  \
        status->ipListRemote += line;                                            \
    }                                                                            \
    for (const auto &line: SplitLinesSkipSharp(dataStore->routing->direct_ip)) { \
        status->ipListDirect += line;                                            \
    }

    QString BuildChainInternal(int chainId, const QList<std::shared_ptr<ProxyEntity>> &ents,
                               const std::shared_ptr<BuildConfigStatus> &status,
                               const QString &forcedTagOut) {
        QString chainTag = "c-" + Int2String(chainId);
        QString chainTagOut;
        bool muxApplied = false;

        QString pastTag;
        int pastExternalStat = 0;
        int index = 0;

        for (const auto &ent: ents) {
            // tagOut: v2ray outbound tag for a profile
            // profile2 (in) (global)   tag g-(id)
            // profile1                 tag (chainTag)-(id)
            // profile0 (out)           tag (chainTag)-(id) / single: chainTag=g-(id)
            auto tagOut = chainTag + "-" + Int2String(ent->id);

            // needGlobal: can only contain one?
            bool needGlobal = false;

            // first profile set as global
            auto isFirstProfile = index == ents.length() - 1;

            if (!forcedTagOut.isEmpty()) {
                // Split-routing extras: fully isolated tags (never share g-<id> with primary)
                if (index == 0) {
                    tagOut = forcedTagOut;
                } else {
                    tagOut = forcedTagOut + QStringLiteral("-h") + Int2String(ent->id);
                }
                needGlobal = false;
            } else {
                if (isFirstProfile) {
                    needGlobal = true;
                    tagOut = "g-" + Int2String(ent->id);
                }

                // last profile set as "proxy"
                if (chainId == 0 && index == 0) {
                    needGlobal = false;
                    tagOut = "proxy";
                }
            }

            // ignoreConnTag
            if (index != 0) {
                status->result->ignoreConnTag << tagOut;
            }

            if (needGlobal) {
                if (status->globalProfiles.contains(ent->id)) {
                    continue;
                }
                status->globalProfiles += ent->id;
            }

            if (index > 0) {
                // chain rules: past
                if (pastExternalStat == 0) {
                    auto replaced = status->outbounds.last().toObject();
                    replaced["detour"] = tagOut;
                    status->outbounds.removeLast();
                    status->outbounds += replaced;
                } else {
                    status->routingRules += QJsonObject{
                        {"inbound", QJsonArray{pastTag + "-mapping"}},
                        {"outbound", tagOut},
                    };
                }
            } else {
                // index == 0 means last profile in chain / not chain
                chainTagOut = tagOut;
                // Do not overwrite primary outboundStat when embedding split-routing extras
                if (forcedTagOut.isEmpty()) {
                    status->result->outboundStat = ent->traffic_data;
                }
            }

            // chain rules: this
            auto ext_mapping_port = 0;
            auto ext_socks_port = 0;
            auto thisExternalStat = ent->bean->NeedExternal(isFirstProfile);
            if (thisExternalStat < 0) {
                status->result->error = "This configuration cannot be set automatically, please try another.";
                return {};
            }

            // determine port
            if (thisExternalStat > 0) {
                if (ent->type == "custom") {
                    auto bean = ent->CustomBean();
                    if (IsValidPort(bean->mapping_port)) {
                        ext_mapping_port = bean->mapping_port;
                    } else {
                        ext_mapping_port = MkPort();
                    }
                    if (IsValidPort(bean->socks_port)) {
                        ext_socks_port = bean->socks_port;
                    } else {
                        ext_socks_port = MkPort();
                    }
                } else {
                    ext_mapping_port = MkPort();
                    ext_socks_port = MkPort();
                }
            }
            if (thisExternalStat == 2) dataStore->need_keep_vpn_off = true;
            if (thisExternalStat == 1) {
                // mapping
                status->inbounds += QJsonObject{
                    {"type", "direct"},
                    {"tag", tagOut + "-mapping"},
                    {"listen", "127.0.0.1"},
                    {"listen_port", ext_mapping_port},
                    {"override_address", ent->bean->serverAddress},
                    {"override_port", ent->bean->serverPort},
                };
                // no chain rule and not outbound, so need to set to direct
                if (isFirstProfile) {
                    status->routingRules += QJsonObject{
                        {"inbound", QJsonArray{tagOut + "-mapping"}},
                        {"outbound", "direct"},
                    };
                }
            }

            // Outbound

            QJsonObject outbound;
            auto stream = GetStreamSettings(ent->bean.get());

            if (thisExternalStat > 0) {
                auto extR = ent->bean->BuildExternal(ext_mapping_port, ext_socks_port, thisExternalStat);
                if (extR.program.isEmpty()) {
                    status->result->error = QObject::tr("Core not found: %1").arg(ent->bean->DisplayCoreType());
                    return {};
                }
                if (!extR.error.isEmpty()) { // rejected
                    status->result->error = extR.error;
                    return {};
                }
                extR.tag = ent->bean->DisplayType();
                status->result->extRs.emplace_back(std::make_shared<NekoGui_fmt::ExternalBuildResult>(extR));

                // SOCKS OUTBOUND
                outbound["type"] = "socks";
                outbound["server"] = "127.0.0.1";
                outbound["server_port"] = ext_socks_port;
            } else {
                const auto coreR = ent->bean->BuildCoreObjSingBox();
                if (coreR.outbound.isEmpty()) {
                    status->result->error = "unsupported outbound";
                    return {};
                }
                if (!coreR.error.isEmpty()) { // rejected
                    status->result->error = coreR.error;
                    return {};
                }
                outbound = coreR.outbound;
            }

            // outbound misc
            outbound["tag"] = tagOut;
            ent->traffic_data->id = ent->id;
            ent->traffic_data->tag = tagOut.toStdString();
            status->result->outboundStats += ent->traffic_data;

            // mux common
            auto needMux = ent->type == "vmess" || ent->type == "trojan" || ent->type == "vless";
            needMux &= dataStore->mux_concurrency > 0;

            if (stream != nullptr) {
                if (stream->network == "grpc" || stream->network == "quic" || (stream->network == "http" && stream->security == "tls")) {
                    needMux = false;
                }
                if (stream->multiplex_status == 0) {
                    if (!dataStore->mux_default_on) needMux = false;
                } else if (stream->multiplex_status == 1) {
                    needMux = true;
                } else if (stream->multiplex_status == 2) {
                    needMux = false;
                }
            }
            if (ent->type == "vless" && outbound["flow"] != "") {
                needMux = false;
            }

            // common
            // apply domain_strategy
            outbound["domain_strategy"] = dataStore->routing->outbound_domain_strategy;
            // apply mux
            if (!muxApplied && needMux) {
                auto muxObj = QJsonObject{
                    {"enabled", true},
                    {"protocol", dataStore->mux_protocol},
                    {"padding", dataStore->mux_padding},
                    {"max_streams", dataStore->mux_concurrency},
                };
                outbound["multiplex"] = muxObj;
                muxApplied = true;
            }

            // apply custom outbound settings
            MergeJson(outbound, QString2QJsonObject(ent->bean->custom_outbound));

            // Bypass Lookup for the first profile
            auto serverAddress = ent->bean->serverAddress;

            auto customBean = dynamic_cast<NekoGui_fmt::CustomBean *>(ent->bean.get());
            if (customBean != nullptr && customBean->core == "internal") {
                auto server = QString2QJsonObject(customBean->config_simple)["server"].toString();
                if (!server.isEmpty()) serverAddress = server;
            }

            if (!IsIpAddress(serverAddress)) {
                status->domainListDNSDirect += "full:" + serverAddress;
            }

            status->outbounds += outbound;
            pastTag = tagOut;
            pastExternalStat = thisExternalStat;
            index++;
        }

        return chainTagOut;
    }

    // SingBox

    void BuildConfigSingBox(const std::shared_ptr<BuildConfigStatus> &status) {
        // Log
        status->result->coreConfig["log"] = QJsonObject{{"level", dataStore->log_level}};

        // Inbounds

        // mixed-in
        if (IsValidPort(dataStore->inbound_socks_port) && !status->forTest) {
            QJsonObject inboundObj;
            inboundObj["tag"] = "mixed-in";
            inboundObj["type"] = "mixed";
            inboundObj["listen"] = dataStore->inbound_address;
            inboundObj["listen_port"] = dataStore->inbound_socks_port;
            if (dataStore->routing->sniffing_mode != SniffingMode::DISABLE) {
                inboundObj["sniff"] = true;
                inboundObj["sniff_override_destination"] = dataStore->routing->sniffing_mode == SniffingMode::FOR_DESTINATION;
            }
            if (dataStore->inbound_auth->NeedAuth()) {
                inboundObj["users"] = QJsonArray{
                    QJsonObject{
                        {"username", dataStore->inbound_auth->username},
                        {"password", dataStore->inbound_auth->password},
                    },
                };
            }
            inboundObj["domain_strategy"] = dataStore->routing->domain_strategy;
            status->inbounds += inboundObj;
        }

        // tun-in
        if (dataStore->vpn_internal_tun && dataStore->spmode_vpn && !status->forTest) {
            QJsonObject inboundObj;
            inboundObj["tag"] = "tun-in";
            inboundObj["type"] = "tun";
            inboundObj["interface_name"] = genTunName();
            inboundObj["auto_route"] = true;
            inboundObj["mtu"] = dataStore->vpn_mtu;
            inboundObj["stack"] = Preset::SingBox::VpnImplementation.value(dataStore->vpn_implementation);
            inboundObj["strict_route"] = dataStore->vpn_strict_route;
            QJsonArray tunAddress;
            tunAddress += "172.19.0.1/28";
            if (dataStore->vpn_ipv6) tunAddress += "fdfe:dcba:9876::1/126";
            inboundObj["address"] = tunAddress;
            if (dataStore->routing->sniffing_mode != SniffingMode::DISABLE) {
                inboundObj["sniff"] = true;
                inboundObj["sniff_override_destination"] = dataStore->routing->sniffing_mode == SniffingMode::FOR_DESTINATION;
            }
            inboundObj["domain_strategy"] = dataStore->routing->domain_strategy;
            status->inbounds += inboundObj;
        }

        // Outbounds
        auto tagProxy = BuildChain(0, status);
        if (!status->result->error.isEmpty()) return;
        Q_UNUSED(tagProxy)

        // Extra outbounds for per-app / per-site split routing (p-<id>)
        BuildSplitRoutingOutbounds(status);

        // direct & bypass (block/dns special outbounds removed — use rule actions)
        status->outbounds += QJsonObject{
            {"type", "direct"},
            {"tag", "direct"},
        };
        status->outbounds += QJsonObject{
            {"type", "direct"},
            {"tag", "bypass"},
        };

        // custom inbound
        if (!status->forTest) QJSONARRAY_ADD(status->inbounds, QString2QJsonObject(dataStore->custom_inbound)["inbounds"].toArray())

        status->result->coreConfig.insert("inbounds", status->inbounds);
        status->result->coreConfig.insert("outbounds", status->outbounds);

        // user rule
        if (!status->forTest) {
            DOMAIN_USER_RULE
            IP_USER_RULE
        }

        // sing-box 1.12+: geoip/geosite rule fields are removed → rule_set
        QJsonArray routeRuleSets;
        QSet<QString> routeRuleSetTags;
        auto ensureRuleSet = [&](const QString &kind, const QString &name) -> QString {
            const QString tag = kind + "-" + name;
            if (routeRuleSetTags.contains(tag)) return tag;
            routeRuleSetTags.insert(tag);
            QString url;
            if (kind == "geoip") {
                url = QStringLiteral("https://raw.githubusercontent.com/SagerNet/sing-geoip/rule-set/geoip-%1.srs").arg(name);
            } else {
                url = QStringLiteral("https://raw.githubusercontent.com/SagerNet/sing-geosite/rule-set/geosite-%1.srs").arg(name);
            }
            routeRuleSets += QJsonObject{
                {"tag", tag},
                {"type", "remote"},
                {"format", "binary"},
                {"url", url},
                {"download_detour", "direct"},
            };
            return tag;
        };

        // sing-box common rule object
        auto make_rule = [&](const QStringList &list, bool isIP = false) {
            QJsonObject rule;
            //
            QJsonArray ip_cidr;
            QJsonArray rule_set;
            bool ip_is_private = false;
            //
            QJsonArray domain_keyword;
            QJsonArray domain_subdomain;
            QJsonArray domain_regexp;
            QJsonArray domain_full;
            for (auto item: list) {
                if (isIP) {
                    if (item.startsWith("geoip:")) {
                        auto code = item.mid(QStringLiteral("geoip:").size()).trimmed().toLower();
                        if (code == "private") {
                            ip_is_private = true;
                        } else if (!code.isEmpty()) {
                            rule_set += ensureRuleSet("geoip", code);
                        }
                    } else {
                        ip_cidr += item;
                    }
                } else {
                    if (item.startsWith("geosite:")) {
                        auto code = item.mid(QStringLiteral("geosite:").size()).trimmed();
                        if (!code.isEmpty()) rule_set += ensureRuleSet("geosite", code);
                    } else if (item.startsWith("full:")) {
                        auto d = item.mid(5).trimmed().toLower();
                        if (d.contains("://")) d = d.section("://", 1);
                        if (d.contains('/')) d = d.section('/', 0, 0);
                        if (d.contains(':')) d = d.section(':', 0, 0);
                        if (!d.isEmpty()) domain_full += d;
                    } else if (item.startsWith("domain:")) {
                        auto d = item.mid(7).trimmed().toLower();
                        if (d.contains("://")) d = d.section("://", 1);
                        if (d.contains('/')) d = d.section('/', 0, 0);
                        if (d.contains(':')) d = d.section(':', 0, 0);
                        while (d.startsWith("*.")) d = d.mid(2);
                        while (d.startsWith(".") && d.indexOf('.', 1) > 0) d = d.mid(1);
                        if (!d.isEmpty()) domain_subdomain += d;
                    } else if (item.startsWith("regexp:")) {
                        domain_regexp += item.mid(7).trimmed().toLower();
                    } else if (item.startsWith("keyword:")) {
                        domain_keyword += item.mid(8).trimmed().toLower();
                    } else {
                        auto d = item.trimmed().toLower();
                        if (d.contains("://")) d = d.section("://", 1);
                        if (d.contains('/')) d = d.section('/', 0, 0);
                        if (d.contains(':')) d = d.section(':', 0, 0);
                        while (d.startsWith("*.")) d = d.mid(2);
                        while (d.startsWith(".") && d.indexOf('.', 1) > 0) d = d.mid(1);
                        if (!d.isEmpty()) domain_subdomain += d;
                    }
                }
            }
            if (isIP) {
                if (ip_cidr.isEmpty() && rule_set.isEmpty() && !ip_is_private) return rule;
                if (!ip_cidr.isEmpty()) rule["ip_cidr"] = ip_cidr;
                if (ip_is_private) rule["ip_is_private"] = true;
                if (!rule_set.isEmpty()) rule["rule_set"] = rule_set;
            } else {
                if (domain_keyword.isEmpty() && domain_subdomain.isEmpty() && domain_regexp.isEmpty() && domain_full.isEmpty() && rule_set.isEmpty()) {
                    return rule;
                }
                if (!domain_full.isEmpty()) rule["domain"] = domain_full;
                if (!domain_subdomain.isEmpty()) rule["domain_suffix"] = domain_subdomain; // v2ray Subdomain => sing-box suffix
                if (!domain_keyword.isEmpty()) rule["domain_keyword"] = domain_keyword;
                if (!domain_regexp.isEmpty()) rule["domain_regex"] = domain_regexp;
                if (!rule_set.isEmpty()) rule["rule_set"] = rule_set;
            }
            return rule;
        };

        // final add DNS
        QJsonObject dns;
        QJsonArray dnsServers;
        QJsonArray dnsRules;

        // Remote
        if (!status->forTest)
            dnsServers += QJsonObject{
                {"tag", "dns-remote"},
                {"address_resolver", "dns-local"},
                {"strategy", dataStore->routing->remote_dns_strategy},
                {"address", dataStore->routing->remote_dns},
                {"detour", tagProxy},
            };

        // Direct
        QJsonObject directObj{
            {"tag", "dns-direct"},
            {"address_resolver", "dns-local"},
            {"strategy", dataStore->routing->direct_dns_strategy},
            {"address", dataStore->routing->direct_dns},
            {"detour", "direct"},
        };
        if (dataStore->routing->dns_final_out == "bypass") {
            dnsServers.prepend(directObj);
        } else {
            dnsServers.append(directObj);
        }
        dnsRules.append(QJsonObject{
            {"outbound", "any"},
            {"server", "dns-direct"},
        });

        // block
        if (!status->forTest)
            dnsServers += QJsonObject{
                {"tag", "dns-block"},
                {"address", "rcode://success"},
            };

        // Fakedns
        if (dataStore->fake_dns && dataStore->vpn_internal_tun && dataStore->spmode_vpn && !status->forTest) {
            dnsServers += QJsonObject{
                {"tag", "dns-fake"},
                {"address", "fakeip"},
            };
            dns["fakeip"] = QJsonObject{
                {"enabled", true},
                {"inet4_range", "198.18.0.0/15"},
                {"inet6_range", "fc00::/18"},
            };
        }

        // Underlying 100% Working DNS ?
        dnsServers += QJsonObject{
            {"tag", "dns-local"},
            {"address", BOX_UNDERLYING_DNS},
            {"detour", "direct"},
        };

        // sing-box dns rule object
        auto add_rule_dns = [&](const QStringList &list, const QString &server) {
            auto rule = make_rule(list, false);
            if (rule.isEmpty()) return;
            rule["server"] = server;
            dnsRules += rule;
        };
        add_rule_dns(status->domainListDNSRemote, "dns-remote");
        add_rule_dns(status->domainListDNSDirect, "dns-direct");

        // Feed custom routing domains into DNS rules so direct domains resolve locally
        // and register reverse IP mapping in sing-box
        if (!status->forTest) {
            QStringList customDirectDomains;
            QStringList customRemoteDomains;
            auto scanJsonDomains = [&](const QString &json) {
                if (json.trimmed().isEmpty()) return;
                auto root = QString2QJsonObject(json);
                auto arr = root.value("rules").toArray();
                for (const auto &rv : arr) {
                    if (!rv.isObject()) continue;
                    auto r = rv.toObject();
                    auto out = r.value("outbound").toString();
                    QStringList domains;
                    for (const auto &d: r.value("domain_suffix").toArray()) {
                        auto s = d.toString().trimmed().toLower();
                        if (s.contains("://")) s = s.section("://", 1);
                        if (s.contains('/')) s = s.section('/', 0, 0);
                        if (s.contains(':')) s = s.section(':', 0, 0);
                        while (s.startsWith("*.")) s = s.mid(2);
                        while (s.startsWith(".") && s.indexOf('.', 1) > 0) s = s.mid(1);
                        if (!s.isEmpty()) domains += s;
                    }
                    for (const auto &d: r.value("domain").toArray()) {
                        auto s = d.toString().trimmed().toLower();
                        if (s.contains("://")) s = s.section("://", 1);
                        if (s.contains('/')) s = s.section('/', 0, 0);
                        if (s.contains(':')) s = s.section(':', 0, 0);
                        if (!s.isEmpty()) domains += "full:" + s;
                    }
                    for (const auto &d: r.value("domain_keyword").toArray()) {
                        auto s = d.toString().trimmed().toLower();
                        if (!s.isEmpty()) domains += "keyword:" + s;
                    }
                    if (out == "direct" || out == "bypass") {
                        customDirectDomains += domains;
                    } else if (out == "proxy" || out.startsWith("p-") || out == ProfileOutboundTag(status->ent->id)) {
                        customRemoteDomains += domains;
                    }
                }
            };
            scanJsonDomains(dataStore->routing->custom);
            scanJsonDomains(dataStore->custom_route_global);
            customDirectDomains.removeDuplicates();
            customRemoteDomains.removeDuplicates();

            // Direct domains first so exceptions beat broad proxy rules
            add_rule_dns(customDirectDomains, "dns-direct");
            add_rule_dns(customRemoteDomains, "dns-remote");
        }

        // built-in rules
        if (!status->forTest) {
            dnsRules += QJsonObject{
                {"query_type", QJsonArray{32, 33}},
                {"server", "dns-block"},
            };
            dnsRules += QJsonObject{
                {"domain_suffix", ".lan"},
                {"server", "dns-block"},
            };
        }

        // fakedns rule
        if (dataStore->fake_dns && dataStore->vpn_internal_tun && dataStore->spmode_vpn && !status->forTest) {
            dnsRules += QJsonObject{
                {"inbound", "tun-in"},
                {"server", "dns-fake"},
            };
        }

        dns["servers"] = dnsServers;
        dns["rules"] = dnsRules;
        dns["independent_cache"] = true;

        if (dataStore->routing->use_dns_object) {
            dns = QString2QJsonObject(dataStore->routing->dns_object);
        }
        status->result->coreConfig.insert("dns", dns);

        // Routing

        // dns hijack (rule action; legacy dns outbound removed in sing-box 1.11+)
        if (!status->forTest) {
            status->routingRules += QJsonObject{
                {"protocol", "dns"},
                {"action", "hijack-dns"},
            };
        }

        // sing-box routing rule object
        auto add_rule_route = [&](const QStringList &list, bool isIP, const QString &out) {
            auto rule = make_rule(list, isIP);
            if (rule.isEmpty()) return;
            if (out == "block") {
                rule["action"] = "reject";
            } else {
                rule["outbound"] = out;
            }
            status->routingRules += rule;
        };

        // final add user rule (direct before remote for exceptions)
        add_rule_route(status->domainListBlock, false, "block");
        add_rule_route(status->domainListDirect, false, "bypass");
        add_rule_route(status->domainListRemote, false, tagProxy);
        add_rule_route(status->ipListBlock, true, "block");
        add_rule_route(status->ipListDirect, true, "bypass");
        add_rule_route(status->ipListRemote, true, tagProxy);

        // built-in rules
        status->routingRules += QJsonObject{
            {"network", "udp"},
            {"port", QJsonArray{443}},
            {"action", "reject"},
        };
        status->routingRules += QJsonObject{
            {"network", "udp"},
            {"port", QJsonArray{135, 137, 138, 139, 5353}},
            {"action", "reject"},
        };
        status->routingRules += QJsonObject{
            {"ip_cidr", QJsonArray{"224.0.0.0/3", "ff00::/8"}},
            {"action", "reject"},
        };
        status->routingRules += QJsonObject{
            {"source_ip_cidr", QJsonArray{"224.0.0.0/3", "ff00::/8"}},
            {"action", "reject"},
        };

        // tun user rule
        if (dataStore->vpn_internal_tun && dataStore->spmode_vpn && !status->forTest) {
            auto match_out = dataStore->vpn_rule_white ? "proxy" : "bypass";

            QString process_name_rule = dataStore->vpn_rule_process.trimmed();
            if (!process_name_rule.isEmpty()) {
                auto arr = SplitLinesSkipSharp(process_name_rule);
                QJsonObject rule{{"outbound", match_out},
                                 {"process_name", QList2QJsonArray(arr)}};
                status->routingRules += rule;
            }

            QString cidr_rule = dataStore->vpn_rule_cidr.trimmed();
            if (!cidr_rule.isEmpty()) {
                auto arr = SplitLinesSkipSharp(cidr_rule);
                QJsonObject rule{{"outbound", match_out},
                                 {"ip_cidr", QList2QJsonArray(arr)}};
                status->routingRules += rule;
            }

            auto autoBypassExternalProcessPaths = getAutoBypassExternalProcessPaths(status->result);
            if (!autoBypassExternalProcessPaths.isEmpty()) {
                QJsonObject rule{{"outbound", "bypass"},
                                 {"process_name", QList2QJsonArray(autoBypassExternalProcessPaths)}};
                status->routingRules += rule;
            }

            // Keep core (and GUI) off the TUN path so URL Test temp instances and
            // control-plane dials never loop through the active VPN.
            status->routingRules += QJsonObject{
                {"outbound", "bypass"},
                {"process_name", QJsonArray{"nekobox_core.exe", "nekobox_core", "nekobox.exe", "nekobox"}},
            };
        }

        // geopath assets are optional now (UI autocomplete / legacy); routing uses remote rule-set
        Q_UNUSED(FindCoreAsset("geoip.db"));
        Q_UNUSED(FindCoreAsset("geosite.db"));

        // final add routing rule (normalize legacy special outbounds + deprecated geo fields in custom rules)
        auto routingRules = QString2QJsonObject(dataStore->routing->custom)["rules"].toArray();
        if (status->forTest) routingRules = {};
        if (!status->forTest) QJSONARRAY_ADD(routingRules, QString2QJsonObject(dataStore->custom_route_global)["rules"].toArray())
        QJSONARRAY_ADD(routingRules, status->routingRules)

        // Known outbound tags for split-routing validation
        QSet<QString> outboundTags;
        for (const auto &ob: status->outbounds) {
            auto tag = ob.toObject().value("tag").toString();
            if (!tag.isEmpty()) outboundTags.insert(tag);
        }
        const QString startedProfileTag = ProfileOutboundTag(status->ent->id);

        // Priority buckets (sing-box: first match wins):
        // 0 System / Core rules (hijack-dns, core bypass, reject ports/QUIC)
        // 1 Sites by server (domain → p-*)
        // 2 Direct/Proxy sites (domain → proxy/direct)
        // 3 Apps by server  (process → p-*)
        // 4 Direct/Proxy apps (process → proxy/direct)
        // 5 everything else (custom / system)
        QJsonArray bucketSystem, bucketServerSites, bucketSites, bucketServerApps, bucketApps, bucketOther;

        auto classifyAndPush = [&](QJsonObject rule) {
            auto action = rule.value("action").toString();
            auto out = rule.value("outbound").toString();
            if (out == "block") {
                rule.remove("outbound");
                rule["action"] = "reject";
                action = "reject";
                out.clear();
            } else if (out == "dns-out" || out == "dns" || rule.value("protocol").toString() == "dns") {
                rule.remove("outbound");
                rule["action"] = "hijack-dns";
                action = "hijack-dns";
                out.clear();
            } else if (out == startedProfileTag) {
                rule["outbound"] = "proxy";
                out = "proxy";
            } else if (out.startsWith(QStringLiteral("p-")) && !outboundTags.contains(out)) {
                return; // missing split outbound
            }

            // System rules take absolute highest priority:
            // 1. hijack-dns so DNS traffic from any app is handled by sing-box DNS router
            // 2. nekobox_core bypass to prevent loops
            // 3. system port/protocol rejects (QUIC reject, NetBIOS, multicast)
            if (action == "hijack-dns") {
                bucketSystem += rule;
                return;
            }
            const auto procArray = rule.value("process_name").toArray();
            if (procArray.contains("nekobox_core") || procArray.contains("nekobox") ||
                procArray.contains("nekobox_core.exe") || procArray.contains("nekobox.exe")) {
                bucketSystem += rule;
                return;
            }
            if (action == "reject" && (rule.contains("port") || rule.contains("ip_cidr") || rule.contains("source_ip_cidr"))) {
                bucketSystem += rule;
                return;
            }

            for (const auto &k: {QStringLiteral("geoip"), QStringLiteral("geosite"), QStringLiteral("source_geoip")}) {
                rule.remove(k);
            }
            if (!ruleHasMatchersLeft(rule)) return;

            if (rule.contains("domain_suffix")) {
                QJsonArray cleanSuffix;
                for (const auto &v: rule.value("domain_suffix").toArray()) {
                    auto s = v.toString().trimmed().toLower();
                    if (s.contains("://")) s = s.section("://", 1);
                    if (s.contains('/')) s = s.section('/', 0, 0);
                    if (s.contains(':')) s = s.section(':', 0, 0);
                    while (s.startsWith("*.")) s = s.mid(2);
                    while (s.startsWith(".") && s.indexOf('.', 1) > 0) s = s.mid(1);
                    if (!s.isEmpty()) cleanSuffix += s;
                }
                rule["domain_suffix"] = cleanSuffix;
            }
            if (rule.contains("domain")) {
                QJsonArray cleanDomain;
                for (const auto &v: rule.value("domain").toArray()) {
                    auto s = v.toString().trimmed().toLower();
                    if (s.contains("://")) s = s.section("://", 1);
                    if (s.contains('/')) s = s.section('/', 0, 0);
                    if (s.contains(':')) s = s.section(':', 0, 0);
                    if (!s.isEmpty()) cleanDomain += s;
                }
                rule["domain"] = cleanDomain;
            }
            if (rule.contains("process_name")) {
                QJsonArray cleanProc;
                for (const auto &v: rule.value("process_name").toArray()) {
                    auto p = v.toString().trimmed();
                    if (p.isEmpty()) continue;
                    cleanProc += p;
#ifndef Q_OS_WIN
                    if (p.endsWith(".exe", Qt::CaseInsensitive)) {
                        cleanProc += p.left(p.length() - 4);
                    }
#endif
                }
                rule["process_name"] = cleanProc;
            }

            const bool hasDomain = rule.contains("domain_suffix") || rule.contains("domain") ||
                                   rule.contains("domain_keyword") || rule.contains("domain_regex");
            const bool hasProc = rule.contains("process_name") || rule.contains("process_path");
            const bool isSplit = out.startsWith(QStringLiteral("p-"));
            const bool isBasicOut = (out == "proxy" || out == "direct" || out == "bypass");

            if (hasDomain && !hasProc && isSplit) {
                bucketServerSites += rule;
            } else if (hasProc && !hasDomain && isSplit) {
                bucketServerApps += rule;
            } else if (hasDomain && !hasProc && isBasicOut) {
                bucketSites += rule;
            } else if (hasProc && !hasDomain && isBasicOut) {
                bucketApps += rule;
            } else {
                bucketOther += rule;
            }
        };

        for (const auto &item: routingRules) {
            if (item.isObject()) classifyAndPush(item.toObject());
        }

        QJsonArray normalizedRules;
        QJSONARRAY_ADD(normalizedRules, bucketSystem)
        QJSONARRAY_ADD(normalizedRules, bucketServerSites)
        QJSONARRAY_ADD(normalizedRules, bucketSites)
        QJSONARRAY_ADD(normalizedRules, bucketServerApps)
        QJSONARRAY_ADD(normalizedRules, bucketApps)
        QJSONARRAY_ADD(normalizedRules, bucketOther)
        auto routeObj = QJsonObject{
            {"rules", normalizedRules},
            // forTest: always leave TUN so latency dials use the physical interface
            {"auto_detect_interface", status->forTest || dataStore->spmode_vpn},
            {"find_process", !status->forTest},
        };
        if (!routeRuleSets.isEmpty()) routeObj["rule_set"] = routeRuleSets;
        if (!status->forTest) routeObj["final"] = dataStore->routing->def_outbound;
        if (status->forExport) {
            routeObj.remove("auto_detect_interface");
        }
        status->result->coreConfig.insert("route", routeObj);

        // experimental
        QJsonObject experimentalObj;

        if (!status->forTest && dataStore->core_box_clash_api > 0) {
            QJsonObject clash_api = {
                {"external_controller", "127.0.0.1:" + Int2String(dataStore->core_box_clash_api)},
                {"secret", dataStore->core_box_clash_api_secret},
                {"external_ui", "dashboard"},
            };
            experimentalObj["clash_api"] = clash_api;
        }
        // required for remote rule-set downloads
        if (!routeRuleSets.isEmpty()) {
            experimentalObj["cache_file"] = QJsonObject{{"enabled", true}};
        }

        if (!experimentalObj.isEmpty()) status->result->coreConfig.insert("experimental", experimentalObj);
    }

    QString WriteVPNSingBoxConfig() {
        // tun user rule
        auto match_out = dataStore->vpn_rule_white ? "neko-socks" : "direct";
        auto no_match_out = dataStore->vpn_rule_white ? "direct" : "neko-socks";

        QString process_name_rule = dataStore->vpn_rule_process.trimmed();
        if (!process_name_rule.isEmpty()) {
            auto arr = SplitLinesSkipSharp(process_name_rule);
            QJsonObject rule{{"outbound", match_out},
                             {"process_name", QList2QJsonArray(arr)}};
            process_name_rule = "," + QJsonObject2QString(rule, false);
        }

        QString cidr_rule = dataStore->vpn_rule_cidr.trimmed();
        if (!cidr_rule.isEmpty()) {
            auto arr = SplitLinesSkipSharp(cidr_rule);
            QJsonObject rule{{"outbound", match_out},
                             {"ip_cidr", QList2QJsonArray(arr)}};
            cidr_rule = "," + QJsonObject2QString(rule, false);
        }

        // TODO bypass ext core process path?

        // auth
        QString socks_user_pass;
        if (dataStore->inbound_auth->NeedAuth()) {
            socks_user_pass = R"( "username": "%1", "password": "%2", )";
            socks_user_pass = socks_user_pass.arg(dataStore->inbound_auth->username, dataStore->inbound_auth->password);
        }
        // gen config
        auto configFn = ":/neko/vpn/sing-box-vpn.json";
        if (QFile::exists("vpn/sing-box-vpn.json")) configFn = "vpn/sing-box-vpn.json";
        auto config = ReadFileText(configFn)
                          .replace("//%IPV6_ADDRESS%", dataStore->vpn_ipv6 ? R"(,"fdfe:dcba:9876::1/126")" : "")
                          .replace("//%SOCKS_USER_PASS%", socks_user_pass)
                          .replace("//%PROCESS_NAME_RULE%", process_name_rule)
                          .replace("//%CIDR_RULE%", cidr_rule)
                          .replace("%MTU%", Int2String(dataStore->vpn_mtu))
                          .replace("%STACK%", Preset::SingBox::VpnImplementation.value(dataStore->vpn_implementation))
                          .replace("%TUN_NAME%", genTunName())
                          .replace("%STRICT_ROUTE%", dataStore->vpn_strict_route ? "true" : "false")
                          .replace("%FINAL_OUT%", no_match_out)
                          .replace("%DNS_ADDRESS%", BOX_UNDERLYING_DNS)
                          .replace("%FAKE_DNS_INBOUND%", dataStore->fake_dns ? "tun-in" : "empty")
                          .replace("%PORT%", Int2String(dataStore->inbound_socks_port));
        // write config
        QFile file;
        file.setFileName(QFileInfo(configFn).fileName());
        file.open(QIODevice::ReadWrite | QIODevice::Truncate);
        file.write(config.toUtf8());
        file.close();
        return QFileInfo(file).absoluteFilePath();
    }

    QString WriteVPNLinuxScript(const QString &configPath) {
#ifdef Q_OS_WIN
        return {};
#endif
        // gen script
        auto scriptFn = ":/neko/vpn/vpn-run-root.sh";
        if (QFile::exists("vpn/vpn-run-root.sh")) scriptFn = "vpn/vpn-run-root.sh";
        auto script = ReadFileText(scriptFn)
                          .replace("./nekobox_core", QApplication::applicationDirPath() + "/nekobox_core")
                          .replace("$CONFIG_PATH", configPath);
        // write script
        QFile file2;
        file2.setFileName(QFileInfo(scriptFn).fileName());
        file2.open(QIODevice::ReadWrite | QIODevice::Truncate);
        file2.write(script.toUtf8());
        file2.close();
        return QFileInfo(file2).absoluteFilePath();
    }

} // namespace NekoGui
