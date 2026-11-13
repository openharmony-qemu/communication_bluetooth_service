/*
 * Copyright (C) 2021-2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <thread>
#include <algorithm>
#include <cctype>

#include "bluetooth_a2dp_sink_server.h"
#include "bluetooth_a2dp_source_server.h"
#include "bluetooth_avrcp_ct_server.h"
#include "bluetooth_avrcp_tg_server.h"
#include "bluetooth_ble_advertiser_server.h"
#include "bluetooth_ble_central_manager_server.h"
#include "bluetooth_errorcode.h"
#include "bluetooth_gatt_client_server.h"
#include "bluetooth_gatt_server_server.h"
#include "bluetooth_hfp_ag_server.h"
#include "bluetooth_hfp_hf_server.h"
#include "bluetooth_hid_host_server.h"
#include "bluetooth_host_dumper.h"
#include "bluetooth_log.h"
#include "bluetooth_pan_server.h"
#include "bluetooth_socket_server.h"
#include "bluetooth_utils_server.h"
#include "file_ex.h"
#include "dialog_switch.h"
#include "hisysevent.h"
#include "interface_adapter_manager.h"
#include "permission_manager.h"
#include "bluetooth_host_server.h"


#include "interface_adapter_ble.h"
#include "interface_adapter_classic.h"
#include "interface_profile_manager.h"
#include "ipc_skeleton.h"
#include "raw_address.h"
#include "remote_observer_list.h"
#include "string_ex.h"
#include "system_ability_definition.h"
#include "ipc_types.h"
#include "safe_map.h"

namespace OHOS {
namespace Bluetooth {
using namespace OHOS::bluetooth;
struct BluetoothHostServer::impl {
    impl();
    ~impl();
    void Init();
    void Clear();

    /// sys state observer
    class SystemStateObserver;
    std::unique_ptr<SystemStateObserver> systemStateObserver_ = nullptr;

    /// adapter state observer
    class AdapterStateObserver;
    std::unique_ptr<AdapterStateObserver> observerImp_ = nullptr;

    /// classic observer
    class AdapterClassicObserver;
    std::unique_ptr<AdapterClassicObserver> classicObserverImp_ = nullptr;

    /// classic remote device observer
    class ClassicRemoteDeviceObserver;
    std::unique_ptr<ClassicRemoteDeviceObserver> remoteObserverImp_ = nullptr;

    /// ble observer
    class AdapterBleObserver;
    std::unique_ptr<AdapterBleObserver> bleObserverImp_ = nullptr;

    /// ble remote device observer
    class BlePeripheralCallback;
    std::unique_ptr<BlePeripheralCallback> bleRemoteObserverImp_ = nullptr;

    /// user regist observers
    RemoteObserverList<IBluetoothHostObserver> observers_;
    RemoteObserverList<IBluetoothHostObserver> bleObservers_;
    SafeMap<sptr<IRemoteObject>, uint32_t> observersToken_;
    SafeMap<sptr<IRemoteObject>, uint32_t> bleObserversToken_;
    SafeMap<sptr<IRemoteObject>, int32_t> observersPid_;
    SafeMap<sptr<IRemoteObject>, int32_t> bleObserversPid_;

    /// user regist remote observers
    RemoteObserverList<IBluetoothRemoteDeviceObserver> remoteObservers_;
    SafeMap<sptr<IRemoteObject>, uint32_t> remoteObserversToken_;
    SafeMap<sptr<IRemoteObject>, int32_t> remoteObserversPid_;

    /// user regist remote observers
    RemoteObserverList<IBluetoothBlePeripheralObserver> bleRemoteObservers_;
    SafeMap<sptr<IRemoteObject>, uint32_t> bleRemoteObserversToken_;

    SafeMap<std::string, sptr<IRemoteObject>> servers_;
    SafeMap<std::string, sptr<IRemoteObject>> bleServers_;

    std::vector<sptr<IBluetoothHostObserver>> hostObservers_;
    std::mutex hostObserversMutex;
    std::vector<sptr<IBluetoothRemoteDeviceObserver>> remoteDeviceObservers_;
    std::mutex remoteDeviceObserversMutex;
    std::vector<sptr<IBluetoothHostObserver>> bleAdapterObservers_;
    std::mutex bleAdapterObserversMutex;
    std::vector<sptr<IBluetoothBlePeripheralObserver>> blePeripheralObservers_;
    std::mutex blePeripheralObserversMutex;

private:
    void createServers();
};

class BluetoothHostServer::impl::SystemStateObserver : public ISystemStateObserver {
public:
    SystemStateObserver(BluetoothHostServer::impl *impl) : impl_(impl) {};
    ~SystemStateObserver() override = default;

    void OnSystemStateChange(const BTSystemState state) override
    {
        if (!impl_) {
            HILOGI("failed: impl_ is null");
            return;
        }
        auto classicService = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
        auto bleService = IAdapterManager::GetInstance()->GetBleAdapterInterface();
        switch (state) {
            case BTSystemState::ON:
                /// update service ptr
                if (classicService) {
                    classicService->RegisterClassicAdapterObserver(
                        *(IAdapterClassicObserver *)impl_->classicObserverImp_.get());
                    classicService->RegisterRemoteDeviceObserver(
                        *(IClassicRemoteDeviceObserver *)impl_->remoteObserverImp_.get());
                }
                if (bleService) {
                    bleService->RegisterBleAdapterObserver(
                        *(IAdapterBleObserver *)impl_->bleObserverImp_.get());
                    bleService->RegisterBlePeripheralCallback(
                        *(IBlePeripheralCallback *)impl_->bleRemoteObserverImp_.get());
                }
                break;

            case BTSystemState::OFF:
                if (classicService) {
                    classicService->DeregisterClassicAdapterObserver(
                        *(IAdapterClassicObserver *)impl_->classicObserverImp_.get());
                    classicService->DeregisterRemoteDeviceObserver(
                        *(IClassicRemoteDeviceObserver *)impl_->remoteObserverImp_.get());
                    classicService = nullptr;
                }
                if (bleService) {
                    bleService->DeregisterBleAdapterObserver(
                        *(IAdapterBleObserver *)impl_->bleObserverImp_.get());
                    bleService->DeregisterBlePeripheralCallback(
                        *(IBlePeripheralCallback *)impl_->bleRemoteObserverImp_.get());
                    bleService = nullptr;
                }
                break;
            default:
                break;
        }
    }

private:
    BluetoothHostServer::impl *impl_ = nullptr;
};

class BluetoothHostServer::impl::AdapterStateObserver : public IAdapterStateObserver {
public:
    AdapterStateObserver(BluetoothHostServer::impl *impl) : impl_(impl){};
    ~AdapterStateObserver() override = default;

    static void OnStateChangeV2(BTTransport transport, BTStateID state, sptr<IBluetoothHostObserver> &observer)
    {
        if (transport == BTTransport::ADAPTER_BREDR && state == BTStateID::STATE_TURN_ON) {
            observer->OnBluetoothStateChanged(BluetoothSwitchState::STATE_ON);
        }
        if (transport == BTTransport::ADAPTER_BLE && state == BTStateID::STATE_TURN_OFF) {
            observer->OnBluetoothStateChanged(BluetoothSwitchState::STATE_OFF);
        }
    }

    void OnStateChange(const BTTransport transport, const BTStateID state) override
    {
        if (!impl_) {
            return;
        }
        if (transport == BTTransport::ADAPTER_BREDR) {
            impl_->observers_.ForEach([this, transport, state](sptr<IBluetoothHostObserver> observer) {
                int32_t pid = this->impl_->observersPid_.ReadVal(observer->AsObject());
                if (BluetoothBleCentralManagerServer::IsResourceScheduleControlApp(pid)) {
                    HILOGI("pid:%{public}d is proxy pid, not callback.", pid);
                    return;
                }
                uint32_t tokenId = this->impl_->observersToken_.ReadVal(observer->AsObject());
                if (PermissionManager::VerifyPermission(ACCESS_BLUETOOTH, tokenId) == false) {
                    HILOGE("false, check permission failed");
                } else {
                    observer->OnStateChanged(transport, state);
                    OnStateChangeV2(transport, state, observer);
                }
            });
            if (state == BTStateID::STATE_TURN_ON || state == BTStateID::STATE_TURN_OFF) {
                int32_t pid = IPCSkeleton::GetCallingPid();
                int32_t uid = IPCSkeleton::GetCallingUid();
                HiSysEventWrite(OHOS::HiviewDFX::HiSysEvent::Domain::BT_SERVICE, "BR_SWITCH_STATE",
                    HiviewDFX::HiSysEvent::EventType::STATISTIC, "PID", pid, "UID", uid, "STATE", state);
            }
        } else if (transport == BTTransport::ADAPTER_BLE) {
            impl_->bleObservers_.ForEach([this, transport, state](sptr<IBluetoothHostObserver> observer) {
                int32_t pid = this->impl_->bleObserversPid_.ReadVal(observer->AsObject());
                if (BluetoothBleCentralManagerServer::IsResourceScheduleControlApp(pid)) {
                    HILOGI("pid:%{public}d is proxy pid, not callback.", pid);
                    return;
                }
                uint32_t tokenId = this->impl_->bleObserversToken_.ReadVal(observer->AsObject());
                if (PermissionManager::VerifyPermission(ACCESS_BLUETOOTH, tokenId) == false) {
                    HILOGE("false, check permission failed");
                } else {
                    observer->OnStateChanged(transport, state);
                    OnStateChangeV2(transport, state, observer);
                }
            });
            if (state == BTStateID::STATE_TURN_ON || state == BTStateID::STATE_TURN_OFF) {
                int32_t pid = IPCSkeleton::GetCallingPid();
                int32_t uid = IPCSkeleton::GetCallingUid();
                HiSysEventWrite(OHOS::HiviewDFX::HiSysEvent::Domain::BT_SERVICE, "BLE_SWITCH_STATE",
                    HiviewDFX::HiSysEvent::EventType::STATISTIC, "PID", pid, "UID", uid, "STATE", state);
            }
        }
    }

private:
    BluetoothHostServer::impl *impl_ = nullptr;
    BLUETOOTH_DISALLOW_COPY_AND_ASSIGN(AdapterStateObserver);
};
class BluetoothHostServer::impl::AdapterClassicObserver : public IAdapterClassicObserver {
public:
    AdapterClassicObserver(BluetoothHostServer::impl *impl) : impl_(impl) {};
    ~AdapterClassicObserver() override = default;

    bool CheckDiscoverOrAccessPermission(uint64_t tokenId)
    {
        if (PermissionManager::IsNativeCaller(tokenId) ||
            PermissionManager::GetApiVersion(tokenId) >= API_VERSION_10) {
            if (!PermissionManager::VerifyPermission(ACCESS_BLUETOOTH, tokenId)) {
                HILOGI("check ACCESS_BLUETOOTH permission failed");
                return false;
            }
        } else {
            if (!PermissionManager::VerifyPermission(DISCOVER_BLUETOOTH, tokenId)) {
                HILOGI("check DISCOVER_BLUETOOTH permission failed");
                return false;
            }
        }
        return true;
    }

    void OnDiscoveryStateChanged(const int32_t status) override
    {
        HILOGI("status: %{public}d", status);
        impl_->observers_.ForEach([this, status](sptr<IBluetoothHostObserver> observer) {
            int32_t pid = this->impl_->observersPid_.ReadVal(observer->AsObject());
            if (BluetoothBleCentralManagerServer::IsResourceScheduleControlApp(pid)) {
                HILOGI("pid:%{public}d is proxy pid, not callback.", pid);
                return;
            }
            observer->OnDiscoveryStateChanged(static_cast<int32_t>(status));
        });
        if (status == DISCOVERY_STARTED || status == DISCOVERY_STOPED) {
            int32_t pid = IPCSkeleton::GetCallingPid();
            int32_t uid = IPCSkeleton::GetCallingUid();
            HiSysEventWrite(OHOS::HiviewDFX::HiSysEvent::Domain::BT_SERVICE, "DISCOVERY_STATE",
                HiviewDFX::HiSysEvent::EventType::STATISTIC, "PID", pid, "UID", uid, "STATE", status);
        }
    }

    void OnDiscoveryResult(
        const RawAddress &device, int rssi, const std::string deviceName, int deviceClass) override
    {
        HILOGI("device: %{public}s, rssi: %{public}d, deviceName: %{pubiic}s, deviceClass: %{public}d",
            GET_ENCRYPT_ADDR(device), rssi, deviceName.c_str(), deviceClass);
        impl_->observers_.ForEach([this, device, rssi, deviceName, deviceClass](IBluetoothHostObserver *observer) {
            int32_t pid = this->impl_->observersPid_.ReadVal(observer->AsObject());
            if (BluetoothBleCentralManagerServer::IsResourceScheduleControlApp(pid)) {
                HILOGI("pid:%{public}d is proxy pid, not callback.", pid);
                return;
            }
            uint32_t tokenId = this->impl_->observersToken_.ReadVal(observer->AsObject());
            if (!CheckDiscoverOrAccessPermission(tokenId)) {
                return;
            }
            observer->OnDiscoveryResult(device, rssi, deviceName, deviceClass);
        });
    }

    void OnPairRequested(const BTTransport transport, const RawAddress &device) override
    {
        HILOGI("device: %{public}s", GET_ENCRYPT_ADDR(device));
        impl_->observers_.ForEach(
            [transport, device](IBluetoothHostObserver *observer) { observer->OnPairRequested(transport, device); });
    }

    void OnPairConfirmed(
        const BTTransport transport, const RawAddress &device, int32_t reqType, int32_t number) override
    {
        HILOGI("device: %{public}s, reqType: %{public}d, number: %{public}d",
            GET_ENCRYPT_ADDR(device), reqType, number);
        impl_->observers_.ForEach([this, transport, device, reqType, number](IBluetoothHostObserver *observer) {
            uint32_t tokenId = this->impl_->observersToken_.ReadVal(observer->AsObject());
            if (!CheckDiscoverOrAccessPermission(tokenId)) {
                return;
            }
            observer->OnPairConfirmed(transport, device, reqType, number);
        });
    }

    void OnScanModeChanged(int32_t mode) override
    {
        HILOGI("mode: %{public}d", mode);
        impl_->observers_.ForEach([mode](IBluetoothHostObserver *observer) { observer->OnScanModeChanged(mode); });
    }

    void OnDeviceNameChanged(const std::string &deviceName) override
    {
        HILOGI("deviceName: %{public}s", deviceName.c_str());
        impl_->observers_.ForEach(
            [deviceName](IBluetoothHostObserver *observer) { observer->OnDeviceNameChanged(deviceName); });
    }

    void OnDeviceAddrChanged(const std::string &address) override
    {
        HILOGI("address: %{public}s", GetEncryptAddr(address).c_str());
        impl_->observers_.ForEach(
            [address](IBluetoothHostObserver *observer) { observer->OnDeviceAddrChanged(address); });
    }

private:
    BluetoothHostServer::impl *impl_ = nullptr;
    BLUETOOTH_DISALLOW_COPY_AND_ASSIGN(AdapterClassicObserver);
};
class BluetoothHostServer::impl::ClassicRemoteDeviceObserver : public IClassicRemoteDeviceObserver {
public:
    ClassicRemoteDeviceObserver(BluetoothHostServer::impl *impl) : impl_(impl) {};
    ~ClassicRemoteDeviceObserver() override = default;

    bool CheckDiscoverOrAccessPermission(uint64_t tokenId)
    {
        if (PermissionManager::IsNativeCaller(tokenId) ||
            PermissionManager::GetApiVersion(tokenId) >= API_VERSION_10) {
            if (!PermissionManager::VerifyPermission(ACCESS_BLUETOOTH, tokenId)) {
                HILOGI("check ACCESS_BLUETOOTH permission failed");
                return false;
            }
        } else {
            if (!PermissionManager::VerifyPermission(DISCOVER_BLUETOOTH, tokenId)) {
                HILOGI("check DISCOVER_BLUETOOTH permission failed");
                return false;
            }
        }
        return true;
    }

    void OnAclStateChanged(const RawAddress &device, int state, unsigned int reason) override
    {
        return;
    }

    void OnPairStatusChanged(const BTTransport transport, const RawAddress &device, const int32_t status) override
    {
        HILOGI("device: %{public}s, status: %{public}d", GET_ENCRYPT_ADDR(device), status);
        impl_->remoteObservers_.ForEach([this, transport, device, status](IBluetoothRemoteDeviceObserver *observer) {
            int32_t pid = this->impl_->remoteObserversPid_.ReadVal(observer->AsObject());
            if (BluetoothBleCentralManagerServer::IsResourceScheduleControlApp(pid)) {
                HILOGI("pid:%{public}d is proxy pid, not callback.", pid);
                return;
            }
            uint32_t tokenId = this->impl_->remoteObserversToken_.ReadVal(observer->AsObject());
            if (!CheckDiscoverOrAccessPermission(tokenId)) {
                return;
            }
            observer->OnPairStatusChanged(transport, device, status, PAIR_COMMON_BOND_CAUSE, BOND_MSG_NO_ERROR);
        });
    }

    void OnRemoteUuidChanged(const RawAddress &device, const std::vector<Uuid> &uuids) override
    {
        HILOGI("device: %{public}s", GET_ENCRYPT_ADDR(device));
        std::vector<bluetooth::Uuid> btUuids;
        for (const auto &val : uuids) {
            btUuids.push_back(val);
        }
        impl_->remoteObservers_.ForEach([this, device, btUuids](IBluetoothRemoteDeviceObserver *observer) {
            int32_t pid = this->impl_->remoteObserversPid_.ReadVal(observer->AsObject());
            if (BluetoothBleCentralManagerServer::IsResourceScheduleControlApp(pid)) {
                HILOGI("pid:%{public}d is proxy pid, not callback.", pid);
                return;
            }
            observer->OnRemoteUuidChanged(device, btUuids);
        });
    }

    void OnRemoteNameChanged(const RawAddress &device, const std::string &deviceName) override
    {
        HILOGI("device: %{public}s, deviceName: %{public}s", GET_ENCRYPT_ADDR(device), deviceName.c_str());
        impl_->remoteObservers_.ForEach([this, device, deviceName](IBluetoothRemoteDeviceObserver *observer) {
            int32_t pid = this->impl_->remoteObserversPid_.ReadVal(observer->AsObject());
            if (BluetoothBleCentralManagerServer::IsResourceScheduleControlApp(pid)) {
                HILOGI("pid:%{public}d is proxy pid, not callback.", pid);
                return;
            }
            observer->OnRemoteNameChanged(device, deviceName);
        });
    }

    void OnRemoteAliasChanged(const RawAddress &device, const std::string &alias) override
    {
        HILOGI("device: %{public}s, alias: %{public}s", GET_ENCRYPT_ADDR(device), alias.c_str());
        impl_->remoteObservers_.ForEach([device, alias](IBluetoothRemoteDeviceObserver *observer) {
            observer->OnRemoteAliasChanged(device, alias);
        });
    }

    void OnRemoteCodChanged(const RawAddress &device, int32_t cod) override
    {
        HILOGI("device: %{public}s, cod: %{public}d", GET_ENCRYPT_ADDR(device), cod);
        impl_->remoteObservers_.ForEach([this, device, cod](IBluetoothRemoteDeviceObserver *observer) {
            int32_t pid = this->impl_->remoteObserversPid_.ReadVal(observer->AsObject());
            if (BluetoothBleCentralManagerServer::IsResourceScheduleControlApp(pid)) {
                HILOGI("pid:%{public}d is proxy pid, not callback.", pid);
                return;
            }
            observer->OnRemoteCodChanged(device, cod);
        });
    }

private:
    BluetoothHostServer::impl *impl_ = nullptr;
    BLUETOOTH_DISALLOW_COPY_AND_ASSIGN(ClassicRemoteDeviceObserver);
};

class BluetoothHostServer::impl::AdapterBleObserver : public IAdapterBleObserver {
public:
    AdapterBleObserver(BluetoothHostServer::impl *impl) : impl_(impl){};
    ~AdapterBleObserver() override = default;

    bool CheckDiscoverOrAccessPermission(uint64_t tokenId)
    {
        if (PermissionManager::IsNativeCaller(tokenId) ||
            PermissionManager::GetApiVersion(tokenId) >= API_VERSION_10) {
            if (!PermissionManager::VerifyPermission(ACCESS_BLUETOOTH, tokenId)) {
                HILOGI("check ACCESS_BLUETOOTH permission failed");
                return false;
            }
        } else {
            if (!PermissionManager::VerifyPermission(DISCOVER_BLUETOOTH, tokenId)) {
                HILOGI("check DISCOVER_BLUETOOTH permission failed");
                return false;
            }
        }
        return true;
    }

    void OnDiscoveryStateChanged(const int32_t status) override
    {
        HILOGI("status: %{public}d", status);
        impl_->bleObservers_.ForEach([this, status](sptr<IBluetoothHostObserver> observer) {
            int32_t pid = this->impl_->bleObserversPid_.ReadVal(observer->AsObject());
            if (BluetoothBleCentralManagerServer::IsResourceScheduleControlApp(pid)) {
                HILOGI("pid:%{public}d is proxy pid, not callback.", pid);
                return;
            }
            observer->OnDiscoveryStateChanged(static_cast<int32_t>(status));
        });
    }

    void OnDiscoveryResult(
        const RawAddress &device, int rssi, const std::string deviceName, int deviceClass) override
    {
        HILOGI("device: %{public}s, rssi: %{public}d, deviceName: %{pubiic}s, deviceClass: %{public}d",
            GET_ENCRYPT_ADDR(device), rssi, deviceName.c_str(), deviceClass);
        impl_->bleObservers_.ForEach([this, device, rssi, deviceName, deviceClass](IBluetoothHostObserver *observer) {
            int32_t pid = this->impl_->bleObserversPid_.ReadVal(observer->AsObject());
            if (BluetoothBleCentralManagerServer::IsResourceScheduleControlApp(pid)) {
                HILOGI("pid:%{public}d is proxy pid, not callback.", pid);
                return;
            }
            uint32_t tokenId = this->impl_->bleObserversToken_.ReadVal(observer->AsObject());
            if (!CheckDiscoverOrAccessPermission(tokenId)) {
                return;
            }
            observer->OnDiscoveryResult(device, rssi, deviceName, deviceClass);
        });
    }

    void OnPairRequested(const BTTransport transport, const RawAddress &device) override
    {
        HILOGI("device: %{public}s", GET_ENCRYPT_ADDR(device));
        impl_->bleObservers_.ForEach(
            [transport, device](IBluetoothHostObserver *observer) { observer->OnPairRequested(transport, device); });
    }

    void OnPairConfirmed(
        const BTTransport transport, const RawAddress &device, const int32_t reqType, const int32_t number) override
    {
        HILOGI("device: %{public}s, reqType: %{public}d, number: %{public}d",
            GET_ENCRYPT_ADDR(device), reqType, number);
        impl_->bleObservers_.ForEach([this, transport, device, reqType, number](IBluetoothHostObserver *observer) {
            int32_t pid = this->impl_->bleObserversPid_.ReadVal(observer->AsObject());
            if (BluetoothBleCentralManagerServer::IsResourceScheduleControlApp(pid)) {
                HILOGI("pid:%{public}d is proxy pid, not callback.", pid);
                return;
            }
            uint32_t tokenId = this->impl_->bleObserversToken_.ReadVal(observer->AsObject());
            if (!CheckDiscoverOrAccessPermission(tokenId)) {
                return;
            }
            observer->OnPairConfirmed(transport, device, reqType, number);
        });
    }

    void OnScanModeChanged(const int32_t mode) override
    {
        HILOGI("mode: %{public}d", mode);
        impl_->bleObservers_.ForEach([mode](IBluetoothHostObserver *observer) { observer->OnScanModeChanged(mode); });
    }

    void OnDeviceNameChanged(const std::string deviceName) override
    {
        HILOGI("deviceName: %{public}s", deviceName.c_str());
        impl_->bleObservers_.ForEach(
            [deviceName](IBluetoothHostObserver *observer) { observer->OnDeviceNameChanged(deviceName); });
    }

    void OnDeviceAddrChanged(const std::string address) override
    {
        HILOGI("address: %{public}s", GetEncryptAddr(address).c_str());
        impl_->bleObservers_.ForEach(
            [address](IBluetoothHostObserver *observer) { observer->OnDeviceAddrChanged(address); });
    }

    void OnAdvertisingStateChanged(const int32_t state) override
    {}

private:
    BluetoothHostServer::impl *impl_ = nullptr;
    BLUETOOTH_DISALLOW_COPY_AND_ASSIGN(AdapterBleObserver);
};
class BluetoothHostServer::impl::BlePeripheralCallback : public IBlePeripheralCallback {
public:
    BlePeripheralCallback(BluetoothHostServer::impl *impl) : impl_(impl) {};
    ~BlePeripheralCallback() override = default;

    void OnReadRemoteRssiEvent(const RawAddress &device, int32_t rssi, int32_t status) override
    {
        HILOGI("device: %{public}s, rssi: %{public}d, status: %{public}d",
            GET_ENCRYPT_ADDR(device), rssi, status);
        impl_->bleRemoteObservers_.ForEach([device, rssi, status](IBluetoothBlePeripheralObserver *observer) {
            observer->OnReadRemoteRssiEvent(device, rssi, status);
        });
    }

    void OnPairStatusChanged(const BTTransport transport, const RawAddress &device, int32_t status) override
    {
        HILOGI("device: %{public}s, status: %{public}d", GET_ENCRYPT_ADDR(device), status);
        impl_->bleRemoteObservers_.ForEach([this, transport, device, status](
            IBluetoothBlePeripheralObserver *observer) {
            uint32_t tokenId = this->impl_->bleRemoteObserversToken_.ReadVal(observer->AsObject());
            if (PermissionManager::VerifyPermission(ACCESS_BLUETOOTH, tokenId) == false) {
                HILOGE("false, check permission failed");
            } else {
                observer->OnPairStatusChanged(transport, device, status, PAIR_COMMON_BOND_CAUSE, BOND_MSG_NO_ERROR);
            }
        });
    }

    void OnAclStateChanged(const RawAddress &device, int state, unsigned int reason) override
    {
        return;
    }

private:
    BluetoothHostServer::impl *impl_ = nullptr;
    BLUETOOTH_DISALLOW_COPY_AND_ASSIGN(BlePeripheralCallback);
};

std::mutex BluetoothHostServer::instanceLock;
sptr<BluetoothHostServer> BluetoothHostServer::instance;

const bool REGISTER_RESULT = SystemAbility::MakeAndRegisterAbility(BluetoothHostServer::GetInstance().GetRefPtr());

BluetoothHostServer::impl::impl()
{
    HILOGI("starts");
    systemStateObserver_ = std::make_unique<SystemStateObserver>(this);
    observerImp_ = std::make_unique<AdapterStateObserver>(this);
    classicObserverImp_ = std::make_unique<AdapterClassicObserver>(this);
    remoteObserverImp_ = std::make_unique<ClassicRemoteDeviceObserver>(this);
    bleObserverImp_ = std::make_unique<AdapterBleObserver>(this);
    bleRemoteObserverImp_ = std::make_unique<BlePeripheralCallback>(this);
}

BluetoothHostServer::impl::~impl()
{
    HILOGI("starts");
}

void BluetoothHostServer::impl::Init()
{
    HILOGI("starts");
    IAdapterManager::GetInstance()->RegisterSystemStateObserver(*systemStateObserver_);

    IAdapterManager::GetInstance()->Start();
    IAdapterManager::GetInstance()->RegisterStateObserver(*observerImp_);

    auto classicService = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
    auto bleService = IAdapterManager::GetInstance()->GetBleAdapterInterface();
    if (classicService) {
        classicService->RegisterClassicAdapterObserver(*classicObserverImp_.get());
        classicService->RegisterRemoteDeviceObserver(*remoteObserverImp_.get());
    }
    if (bleService) {
        bleService->RegisterBleAdapterObserver(*bleObserverImp_.get());
        bleService->RegisterBlePeripheralCallback(*bleRemoteObserverImp_.get());
    }

    createServers();
}

void BluetoothHostServer::impl::Clear()
{
    /// systerm state observer
    IAdapterManager::GetInstance()->DeregisterSystemStateObserver(*systemStateObserver_);

    /// adapter state observer
    IAdapterManager::GetInstance()->Stop();
    IAdapterManager::GetInstance()->DeregisterStateObserver(*observerImp_);

    auto classicService = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
    if (classicService) {
        /// classic observer
        classicService->DeregisterClassicAdapterObserver(*classicObserverImp_.get());
        /// classic remote observer
        classicService->DeregisterRemoteDeviceObserver(*remoteObserverImp_.get());
    }

    auto bleService = IAdapterManager::GetInstance()->GetBleAdapterInterface();
    if (bleService) {
        /// ble observer
        bleService->DeregisterBleAdapterObserver(*bleObserverImp_.get());
        /// ble remote observer
        bleService->DeregisterBlePeripheralCallback(*bleRemoteObserverImp_.get());
    }
}

void BluetoothHostServer::impl::createServers()
{
    sptr<BluetoothSocketServer> socket = new BluetoothSocketServer();
    servers_.EnsureInsert(PROFILE_SOCKET, socket->AsObject());

    sptr<BluetoothGattServerServer> gattserver = new BluetoothGattServerServer();
    servers_.EnsureInsert(PROFILE_GATT_SERVER, gattserver->AsObject());

    sptr<BluetoothGattClientServer> gattclient = new BluetoothGattClientServer();
    servers_.EnsureInsert(PROFILE_GATT_CLIENT, gattclient->AsObject());

#ifdef BLUETOOTH_HFP_AG_FEATURE
    sptr<BluetoothHfpAgServer> hfpAg = new BluetoothHfpAgServer();
    servers_.EnsureInsert(PROFILE_HFP_AG, hfpAg->AsObject());
#endif

#ifdef BLUETOOTH_HFP_HF_FEATURE
    sptr<BluetoothHfpHfServer> hfpHf = new BluetoothHfpHfServer();
    servers_.EnsureInsert(PROFILE_HFP_HF, hfpHf->AsObject());
#endif

#ifdef BLUETOOTH_AVRCP_CT_FEATURE
    sptr<BluetoothAvrcpCtServer> avrcpCtServer = new BluetoothAvrcpCtServer();
    servers_.EnsureInsert(PROFILE_AVRCP_CT, avrcpCtServer->AsObject());
#endif

#ifdef BLUETOOTH_AVRCP_TG_FEATURE
    sptr<BluetoothAvrcpTgServer> avrcpTgServer = new BluetoothAvrcpTgServer();
    servers_.EnsureInsert(PROFILE_AVRCP_TG, avrcpTgServer->AsObject());
#endif

    sptr<BluetoothBleAdvertiserServer> bleAdvertiser = new BluetoothBleAdvertiserServer();
    bleServers_.EnsureInsert(BLE_ADVERTISER_SERVER, bleAdvertiser->AsObject());

    sptr<BluetoothBleCentralManagerServer> bleCentralManger = new BluetoothBleCentralManagerServer();
    bleServers_.EnsureInsert(BLE_CENTRAL_MANAGER_SERVER, bleCentralManger->AsObject());

#ifdef BLUETOOTH_MAP_SERVER_FEATURE
    sptr<BluetoothMapMceServer> mapMce = new BluetoothMapMceServer();
    servers_.EnsureInsert(PROFILE_MAP_MCE, mapMce->AsObject());
#endif

#ifdef BLUETOOTH_MAP_CLIENT_FEATURE
    sptr<BluetoothMapMseServer> mapMse = new BluetoothMapMseServer();
    servers_.EnsureInsert(PROFILE_MAP_MSE, mapMse->AsObject());
#endif

#ifdef BLUETOOTH_A2DP_SRC_FEATURE
    sptr<BluetoothA2dpSourceServer> a2dpSource = new BluetoothA2dpSourceServer();
    servers_.EnsureInsert(PROFILE_A2DP_SRC, a2dpSource->AsObject());
#endif

#ifdef BLUETOOTH_A2DP_SINK_FEATURE
    sptr<BluetoothA2dpSinkServer> a2dpSink = new BluetoothA2dpSinkServer();
    servers_.EnsureInsert(PROFILE_A2DP_SINK, a2dpSink->AsObject());
#endif

#ifdef BLUETOOTH_HID_HOST_FEATURE
    sptr<BluetoothHidHostServer> hidHostServer = new BluetoothHidHostServer();
    servers_.EnsureInsert(PROFILE_HID_HOST_SERVER, hidHostServer->AsObject());
#endif

#ifdef BLUETOOTH_PAN_FEATURE
    sptr<BluetoothPanServer> panServer = new BluetoothPanServer();
    servers_.EnsureInsert(PROFILE_PAN_SERVER, panServer->AsObject());
#endif

    HILOGI("servers_ constructed, size is %{public}d", servers_.Size());
}

BluetoothHostServer::BluetoothHostServer() : SystemAbility(BLUETOOTH_HOST_SYS_ABILITY_ID, true)
{
    pimpl = std::make_unique<impl>();
}

BluetoothHostServer::~BluetoothHostServer()
{
    HILOGI("~BluetoothHostServer called.");
}

sptr<BluetoothHostServer> BluetoothHostServer::GetInstance()
{
    std::lock_guard<std::mutex> autoLock(instanceLock);
    if (instance == nullptr) {
        sptr<BluetoothHostServer> temp = new BluetoothHostServer();
        instance = temp;
    }
    return instance;
}

void BluetoothHostServer::OnStart()
{
    HILOGI("starting service.");
    if (state_ == ServiceRunningState::STATE_RUNNING) {
        HILOGI("service is already started.");
        return;
    }

    if (!Init()) {
        HILOGE("initiation failed");
        OnStop();
        return;
    }

    state_ = ServiceRunningState::STATE_RUNNING;

    HILOGI("service has been started successfully");
    return;
}

bool BluetoothHostServer::Init()
{
    pimpl->Init();
    if (!registeredToService_) {
        bool ret = Publish(BluetoothHostServer::GetInstance());
        if (!ret) {
            HILOGE("init publish failed!");
            return false;
        }
        registeredToService_ = true;
    }
    HILOGI("init success");
    return true;
}

void BluetoothHostServer::OnStop()
{
    HILOGI("stopping service.");

    pimpl->Clear();
    state_ = ServiceRunningState::STATE_IDLE;
    registeredToService_ = false;
    return;
}

void BluetoothHostServer::RegisterObserver(const sptr<IBluetoothHostObserver> &observer)
{
    if (observer == nullptr) {
        HILOGE("RegisterObserver observer is null");
        return;
    }

    pimpl->observersToken_.EnsureInsert(observer->AsObject(), IPCSkeleton::GetCallingTokenID());
    pimpl->observersPid_.EnsureInsert(observer->AsObject(), IPCSkeleton::GetCallingUid());
    auto func = std::bind(&BluetoothHostServer::DeregisterObserver, this, std::placeholders::_1);
    pimpl->observers_.Register(observer, func);
    std::lock_guard<std::mutex> lock(pimpl->hostObserversMutex);
    pimpl->hostObservers_.push_back(observer);
}

void BluetoothHostServer::DeregisterObserver(const sptr<IBluetoothHostObserver> &observer)
{
    if (observer == nullptr || pimpl == nullptr) {
        HILOGE("DeregisterObserver observer is null");
        return;
    }
    {
        std::lock_guard<std::mutex> lock(pimpl->hostObserversMutex);
        for (auto iter = pimpl->hostObservers_.begin(); iter != pimpl->hostObservers_.end(); ++iter) {
            if ((*iter)->AsObject() == observer->AsObject()) {
                pimpl->observers_.Deregister(*iter);
                pimpl->hostObservers_.erase(iter);
                break;
            }
        }
    }
    pimpl->observersToken_.Erase(observer->AsObject());
    pimpl->observersPid_.Erase(observer->AsObject());
}

int32_t BluetoothHostServer::EnableBt()
{
    if (IAdapterManager::GetInstance()->Enable(bluetooth::BTTransport::ADAPTER_BREDR)) {
        return NO_ERROR;
    }
    return BT_ERR_INTERNAL_ERROR;
}

int32_t BluetoothHostServer::DisableBt(bool isAsync, const std::string &callingName)
{
    (void)(callingName);
    if (isAsync) {
        return BT_ERR_API_NOT_SUPPORT;
    }
    if (IAdapterManager::GetInstance()->Disable(bluetooth::BTTransport::ADAPTER_BREDR)) {
        return NO_ERROR;
    }
    return BT_ERR_INTERNAL_ERROR;
}

static int32_t ConvertBTStateIDToBluetoothState(int32_t brState, int32_t bleState)
{
    BluetoothState state = BluetoothState::STATE_OFF;
    switch (brState) {
        case BTStateID::STATE_TURN_ON: state = BluetoothState::STATE_ON; break;
        case BTStateID::STATE_TURNING_ON: state = BluetoothState::STATE_TURNING_ON; break;
        case BTStateID::STATE_TURNING_OFF: state = BluetoothState::STATE_TURNING_OFF; break;
        case BTStateID::STATE_TURN_OFF: {
            switch (bleState) {
                case BTStateID::STATE_TURN_ON: state = BluetoothState::STATE_BLE_ON; break;
                case BTStateID::STATE_TURNING_ON: state = BluetoothState::STATE_BLE_TURNING_ON; break;
                case BTStateID::STATE_TURNING_OFF: state = BluetoothState::STATE_BLE_TURNING_OFF; break;
                default: break;
            }
        }
        default: HILOGE("Invalid bt state"); break;
    }
    return static_cast<int32_t>(state);
}

int32_t BluetoothHostServer::GetBtState(int32_t &state)
{
    int32_t brState = IAdapterManager::GetInstance()->GetState(bluetooth::BTTransport::ADAPTER_BREDR);
    int32_t bleState = IAdapterManager::GetInstance()->GetState(bluetooth::BTTransport::ADAPTER_BLE);
    state = ConvertBTStateIDToBluetoothState(brState, bleState);
    return BT_NO_ERROR;
}

sptr<IRemoteObject> BluetoothHostServer::GetProfile(const std::string &name)
{
    HILOGI("seraching %{public}s ", name.c_str());
    sptr<IRemoteObject> object = nullptr;
    if (pimpl->servers_.Find(name, object)) {
        return object;
    }
    return object;
}

sptr<IRemoteObject> BluetoothHostServer::GetBleRemote(const std::string &name)
{
    HILOGI("GetBleRemote %{public}s ", name.c_str());
    sptr<IRemoteObject> object = nullptr;
    if (pimpl->bleServers_.Find(name, object)) {
        return object;
    }
    return object;
}

// Fac_Res_CODE
int32_t BluetoothHostServer::BluetoothFactoryReset()
{
    bool ret = IAdapterManager::GetInstance()->FactoryReset();
    return ret ? BT_NO_ERROR : BT_ERR_INTERNAL_ERROR;
}

int32_t BluetoothHostServer::GetDeviceType(int32_t transport, const std::string &address)
{
    HILOGI("transport: %{public}d, address: %{public}s", transport, GetEncryptAddr(address).c_str());
    auto classicService = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
    auto bleService = IAdapterManager::GetInstance()->GetBleAdapterInterface();
    RawAddress addr(address);
    if ((transport == BTTransport::ADAPTER_BREDR) && IsBtEnabled() && classicService) {
        return classicService->GetDeviceType(addr);
    } else if ((transport == BTTransport::ADAPTER_BLE) && IsBleEnabled() && bleService) {
        return bleService->GetDeviceType(addr);
    } else {
        HILOGE("transport invalid or BT current state is not enabled!");
    }
    return INVALID_VALUE;
}

int32_t BluetoothHostServer::GetLocalAddress(std::string &addr)
{
    HILOGI("Enter!");
    auto classicService = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
    auto bleService = IAdapterManager::GetInstance()->GetBleAdapterInterface();
    if (IsBtEnabled() && classicService) {
        addr = classicService->GetLocalAddress();
        return NO_ERROR;
    } else if (IsBleEnabled() && bleService) {
        addr = bleService->GetLocalAddress();
        return NO_ERROR;
    } else {
        HILOGW("BT current state is not enabled!");
        return BT_ERR_INVALID_STATE;
    }
}

int32_t BluetoothHostServer::GenerateLocalOobData(int32_t transport, const sptr<IBluetoothOobObserver> &observer)
{
    if (transport != BT_TRANSPORT_BREDR && transport != BT_TRANSPORT_BLE) {
        return BT_ERR_INVALID_PARAM;
    }
    if (observer == nullptr) {
        return BT_ERR_INVALID_PARAM;
    }
    if ((transport == BT_TRANSPORT_BREDR && !IsBtEnabled()) ||
        (transport == BT_TRANSPORT_BLE && !IsBleEnabled())) {
        return BT_ERR_INVALID_STATE;
    }
    // No controller generation is currently wired to this observer API.
    return BT_ERR_API_NOT_SUPPORT;
}

int32_t BluetoothHostServer::EnableBle(bool noAutoConnect, bool isAsync, const std::string &callingName)
{
    HILOGI("Enter!");
    (void)(callingName);
    if (isAsync) {
        return BT_ERR_API_NOT_SUPPORT;
    }
    if (!BluetoothHostServer::IsBleEnabled() && IAdapterManager::GetInstance()->Enable(BTTransport::ADAPTER_BLE)) {
        return NO_ERROR;
    }
    return BT_ERR_INTERNAL_ERROR;
}

int32_t BluetoothHostServer::DisableBle()
{
    HILOGI("Enter!");
    if (IAdapterManager::GetInstance()->Disable(BTTransport::ADAPTER_BLE)) {
        return NO_ERROR;
    }
    return BT_ERR_INTERNAL_ERROR;
}

bool BluetoothHostServer::IsBrEnabled()
{
    return IsBtEnabled();
}

bool BluetoothHostServer::IsBleEnabled()
{
    return IAdapterManager::GetInstance()->GetState(BTTransport::ADAPTER_BLE) == BTStateID::STATE_TURN_ON;
}

std::vector<uint32_t> BluetoothHostServer::GetProfileList()
{
    HILOGI("Enter!");
    return IProfileManager::GetInstance()->GetProfileServicesList();
}

int32_t BluetoothHostServer::GetMaxNumConnectedAudioDevices()
{
    HILOGI("Enter!");
    return IAdapterManager::GetInstance()->GetMaxNumConnectedAudioDevices();
}

int32_t BluetoothHostServer::GetBtConnectionState(int32_t &state)
{
    if (IsBtEnabled()) {
        state = (int32_t)IAdapterManager::GetInstance()->GetAdapterConnectState();
        HILOGI("state: %{public}d", state);
        return NO_ERROR;
    } else {
        HILOGW("BT current state is not enabled!");
        return BT_ERR_INVALID_STATE;
    }
}

int32_t BluetoothHostServer::GetBtProfileConnState(uint32_t profileId, int &state)
{
    HILOGI("Enter!");
    if (IsBtEnabled()) {
        state = (int32_t)IProfileManager::GetInstance()->GetProfileServiceConnectState(profileId);
        return NO_ERROR;
    } else {
        HILOGW("BT current state is not enabled!");
        return BT_ERR_INVALID_STATE;
    }
}

void BluetoothHostServer::GetLocalSupportedUuids(std::vector<std::string> &uuids)
{
    HILOGI("Enter!");
    IProfileManager::GetInstance()->GetProfileServicesSupportedUuids(uuids);
}

int32_t BluetoothHostServer::GetLocalDeviceClass()
{
    HILOGI("Enter!");
    auto classicService = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
    if (IsBtEnabled() && classicService) {
        return classicService->GetLocalDeviceClass();
    } else {
        HILOGW("BT current state is not enabled!");
    }
    return 0;
}

bool BluetoothHostServer::SetLocalDeviceClass(const int32_t &deviceClass)
{
    HILOGI("Enter!");
    auto classicService = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
    if (IsBtEnabled() && classicService) {
        return classicService->SetLocalDeviceClass(deviceClass);
    } else {
        HILOGW("BT current state is not enabled!");
    }
    return false;
}

int32_t BluetoothHostServer::GetLocalName(std::string &name)
{
    HILOGI("Enter!");
    auto classicService = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
    auto bleService = IAdapterManager::GetInstance()->GetBleAdapterInterface();
    if (IsBtEnabled() && classicService) {
        name = classicService->GetLocalName();
        return NO_ERROR;
    } else if (IsBleEnabled() && bleService) {
        name = bleService->GetLocalName();
        return NO_ERROR;
    } else {
        HILOGW("BT current state is not enabled!");
        return BT_ERR_INVALID_STATE;
    }
}

int32_t BluetoothHostServer::SetLocalName(const std::string &name)
{
    HILOGI("name: %{public}s", name.c_str());
    if (!PermissionManager::IsSystemHap() && PermissionManager::GetApiVersion() >= API_VERSION_12) {
        return BT_ERR_API_NOT_SUPPORT;
    }
    auto classicService = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
    auto bleService = IAdapterManager::GetInstance()->GetBleAdapterInterface();
    if (IsBtEnabled() && classicService) {
        bool ret = classicService->SetLocalName(name);
        if (ret && (IsBleEnabled() && bleService)) {
            if (bleService->SetLocalName(name)) {
                return NO_ERROR;
            }
        } else {
            HILOGE("failed!");
            return BT_ERR_INTERNAL_ERROR;
        }
    } else if (IsBleEnabled() && bleService) {
        if (bleService->SetLocalName(name)) {
            return NO_ERROR;
        }
    } else {
        HILOGW("BT current state is not enabled!");
        return BT_ERR_INVALID_STATE;
    }
    return BT_ERR_INTERNAL_ERROR;
}

int32_t BluetoothHostServer::GetBtScanMode(int32_t &scanMode)
{
    HILOGI("Enter!");
    auto classicService = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
    if (IsBtEnabled() && classicService) {
        scanMode = classicService->GetBtScanMode();
        return NO_ERROR;
    } else {
        HILOGW("BT current state is not enabled!");
        return BT_ERR_INVALID_STATE;
    }
}

int32_t BluetoothHostServer::SetBtScanMode(int32_t mode, int32_t duration)
{
    HILOGI("mode: %{public}d, duration: %{public}d", mode, duration);
    auto classicService = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
    if (IsBtEnabled() && classicService) {
        if (classicService->SetBtScanMode(mode, duration)) {
            return NO_ERROR;
        }
    } else {
        HILOGW("BT current state is not enabled!");
        return BT_ERR_INVALID_STATE;
    }
    return BT_ERR_INTERNAL_ERROR;
}

int32_t BluetoothHostServer::GetBondableMode(int32_t transport)
{
    HILOGI("transport: %{public}d", transport);
    auto classicService = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
    auto bleService = IAdapterManager::GetInstance()->GetBleAdapterInterface();
    if ((transport == BTTransport::ADAPTER_BREDR) && IsBtEnabled() && classicService) {
        return classicService->GetBondableMode();
    } else if ((transport == BTTransport::ADAPTER_BLE) && IsBleEnabled() && bleService) {
        return bleService->GetBondableMode();
    } else {
        HILOGE("Parameter::transport invalid or BT current state is not enabled!");
    }
    return BONDABLE_MODE_OFF;
}

bool BluetoothHostServer::SetBondableMode(int32_t transport, int32_t mode)
{
    HILOGI("transport: %{public}d, mode: %{public}d", transport, mode);
    auto classicService = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
    auto bleService = IAdapterManager::GetInstance()->GetBleAdapterInterface();
    if ((transport == BTTransport::ADAPTER_BREDR) && IsBtEnabled() && classicService) {
        return classicService->SetBondableMode(mode);
    } else if ((transport == BTTransport::ADAPTER_BLE) && IsBleEnabled() && bleService) {
        return bleService->SetBondableMode(mode);
    } else {
        HILOGE("Parameter::transport invalid or BT current state is not enabled!");
    }
    return false;
}

int32_t BluetoothHostServer::StartBtDiscovery()
{
    HILOGI("Enter!");
    if (PermissionManager::IsNativeCaller()||PermissionManager::GetApiVersion() >= API_VERSION_10) {
        if (PermissionManager::VerifyPermission(ACCESS_BLUETOOTH) == false) {
            HILOGE("check access permission failed.");
            return BT_ERR_PERMISSION_FAILED;
        }
    } else {
        if (PermissionManager::VerifyPermission(DISCOVER_BLUETOOTH) == false ||
            PermissionManager::VerifyPermission(MANAGE_BLUETOOTH) == false) {
            HILOGE("check permission failed.");
            return BT_ERR_PERMISSION_FAILED;
        }
        if (PermissionManager::VerifyPermission(APPROXIMATELY_LOCATION) == false &&
            PermissionManager::VerifyPermission(LOCATION) == false) {
            HILOGE("No location permission");
            return BT_ERR_PERMISSION_FAILED;
        }
    }
    auto classicService = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
    if (IsBtEnabled() && classicService) {
        if (classicService->StartBtDiscovery()) {
            return NO_ERROR;
        }
    } else {
        HILOGW("BT current state is not enabled!");
        return BT_ERR_INVALID_STATE;
    }
    return BT_ERR_INTERNAL_ERROR;
}

int32_t BluetoothHostServer::CancelBtDiscovery()
{
    HILOGI("Enter!");
    auto classicService = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
    if (IsBtEnabled() && classicService) {
        if (classicService->CancelBtDiscovery()) {
            return NO_ERROR;
        }
    } else {
        HILOGW("BT current state is not enabled!");
        return BT_ERR_INVALID_STATE;
    }
    return BT_ERR_INTERNAL_ERROR;
}

int32_t BluetoothHostServer::IsBtDiscovering(bool &isDisCovering, int32_t transport)
{
    HILOGI("transport: %{public}d", transport);
    auto classicService = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
    auto bleService = IAdapterManager::GetInstance()->GetBleAdapterInterface();
    if ((transport == BTTransport::ADAPTER_BREDR) && IsBtEnabled() && classicService) {
        isDisCovering = classicService->IsBtDiscovering();
    } else if ((transport == BTTransport::ADAPTER_BLE) && IsBleEnabled() && bleService) {
        isDisCovering = bleService->IsBtDiscovering();
    } else {
        HILOGE("Parameter::transport invalid or BT current state is not enabled!");
        return BT_ERR_INVALID_STATE;
    }
    return BT_NO_ERROR;
}

long BluetoothHostServer::GetBtDiscoveryEndMillis()
{
    HILOGI("Enter!");
    auto classicService = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
    if (IsBtEnabled() && classicService) {
        return classicService->GetBtDiscoveryEndMillis();
    } else {
        HILOGW("BT current state is not enabled!");
    }
    return INVALID_VALUE;
}

int32_t BluetoothHostServer::GetPairedDevices(std::vector<BluetoothRawAddress> &pairedAddr)
{
    HILOGI("GetPairedDevices");
    auto classicService = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
    auto bleService = IAdapterManager::GetInstance()->GetBleAdapterInterface();
    std::vector<RawAddress> rawAddrVec;
    if (IsBtEnabled() && classicService) {
        rawAddrVec = classicService->GetPairedDevices();
    } else {
        HILOGE("transport invalid or BT current state is not enabled!");
        return BT_ERR_INVALID_STATE;
    }
    for (auto it = rawAddrVec.begin(); it != rawAddrVec.end(); ++it) {
        BluetoothRawAddress rawAddr = BluetoothRawAddress(*it);
        pairedAddr.emplace_back(rawAddr);
    }
    if (IsBleEnabled() && bleService) {
        rawAddrVec = bleService->GetPairedDevices();
    } else {
        HILOGE("transport invalid or BT current state is not enabled!");
        return BT_ERR_INVALID_STATE;
    }

    for (auto it = rawAddrVec.begin(); it != rawAddrVec.end(); ++it) {
        BluetoothRawAddress rawAddr = BluetoothRawAddress(*it);
        pairedAddr.emplace_back(rawAddr);
    }
    return NO_ERROR;
}

int BluetoothHostServer::GetTransportByDeviceType(int32_t transport, const std::string &address)
{
    if (transport == BT_TRANSPORT_NONE) {
        int deviceType = GetDeviceType(BT_TRANSPORT_BREDR, address);
        if (deviceType == INVALID_TYPE || deviceType == DEVICE_TYPE_LE) {
            transport = BT_TRANSPORT_BLE;
        } else {
            transport = BT_TRANSPORT_BREDR;
        }
    }
    return transport;
}

int32_t BluetoothHostServer::RemovePair(int32_t transport, const sptr<BluetoothRawAddress> &device)
{
    if (device == nullptr) {
        HILOGE("device is nullptr.");
        return BT_ERR_INTERNAL_ERROR;
    }
    HILOGI("addr:%{public}s, transport:%{public}d", GET_ENCRYPT_ADDR(*device), transport);
    auto classicService = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
    auto bleService = IAdapterManager::GetInstance()->GetBleAdapterInterface();
    transport = GetTransportByDeviceType(transport, device->GetAddress());
    if ((transport == BTTransport::ADAPTER_BREDR) && IsBtEnabled() && classicService) {
        if (classicService->RemovePair(*device)) {
            return NO_ERROR;
        }
    } else if ((transport == BTTransport::ADAPTER_BLE) && IsBleEnabled() && bleService) {
        if (bleService->RemovePair(*device)) {
            return NO_ERROR;
        }
    } else {
        HILOGE("transport invalid or BT/BLE current state is not enabled!");
        return BT_ERR_INVALID_STATE;
    }
    return BT_ERR_INTERNAL_ERROR;
}

bool BluetoothHostServer::RemoveAllPairs()
{
    HILOGI("Enter!");
    if (BTStateID::STATE_TURN_ON != IAdapterManager::GetInstance()->GetState(BTTransport::ADAPTER_BREDR) &&
        BTStateID::STATE_TURN_ON != IAdapterManager::GetInstance()->GetState(BTTransport::ADAPTER_BLE)) {
        HILOGW("BT current state is not enabled!");
        return false;
    }

    auto classicService = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
    bool ret = true;
    if (IsBtEnabled() && classicService) {
        ret = classicService->RemoveAllPairs();
        if (!ret) {
            HILOGE("BREDR RemoveAllPairs failed");
        }
    }

    auto bleService = IAdapterManager::GetInstance()->GetBleAdapterInterface();
    if (IsBleEnabled() && bleService) {
        ret &= bleService->RemoveAllPairs();
        if (!ret) {
            HILOGE("BLE RemoveAllPairs failed");
        }
    }
    return ret;
}

int32_t BluetoothHostServer::GetBleMaxAdvertisingDataLength()
{
    HILOGI("Enter!");
    auto bleService = IAdapterManager::GetInstance()->GetBleAdapterInterface();
    if (IsBleEnabled() && bleService) {
        return bleService->GetBleMaxAdvertisingDataLength();
    } else {
        HILOGW("BT current state is not enabled!");
    }
    return INVALID_VALUE;
}

int32_t BluetoothHostServer::GetPhonebookPermission(const std::string &address)
{
    HILOGI("address: %{public}s", GetEncryptAddr(address).c_str());
    return (int32_t)IAdapterManager::GetInstance()->GetPhonebookPermission(address);
}

bool BluetoothHostServer::SetPhonebookPermission(const std::string &address, int32_t permission)
{
    HILOGI("address: %{public}s, permission: %{public}d", GetEncryptAddr(address).c_str(), permission);
    return IAdapterManager::GetInstance()->SetPhonebookPermission(address, (BTPermissionType)permission);
}

int32_t BluetoothHostServer::GetMessagePermission(const std::string &address)
{
    HILOGI("address: %{public}s", GetEncryptAddr(address).c_str());
    return (int32_t)IAdapterManager::GetInstance()->GetMessagePermission(address);
}

bool BluetoothHostServer::SetMessagePermission(const std::string &address, int32_t permission)
{
    HILOGI("address: %{public}s, permission: %{public}d", GetEncryptAddr(address).c_str(), permission);
    return IAdapterManager::GetInstance()->SetMessagePermission(address, (BTPermissionType)permission);
}

int32_t BluetoothHostServer::GetPowerMode(const std::string &address)
{
    HILOGI("address: %{public}s", GetEncryptAddr(address).c_str());
    return IAdapterManager::GetInstance()->GetPowerMode(address);
}

int32_t BluetoothHostServer::GetDeviceName(int32_t transport, const std::string &address, std::string &name, bool alias)
{
    auto classicService = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
    auto bleService = IAdapterManager::GetInstance()->GetBleAdapterInterface();
    RawAddress addr(address);
    transport = GetTransportByDeviceType(transport, address);
    if ((transport == BT_TRANSPORT_BREDR) && IsBtEnabled() && classicService) {
        name = classicService->GetDeviceName(addr);
        return NO_ERROR;
    } else if ((transport == BT_TRANSPORT_BLE) && IsBleEnabled() && bleService) {
        name = bleService->GetDeviceName(addr);
        return NO_ERROR;
    } else {
        HILOGE("transport invalid or BT current state is not enabled!");
        return BT_ERR_INVALID_STATE;
    }
}

std::string BluetoothHostServer::GetDeviceAlias(const std::string &address)
{
    HILOGI("address: %{public}s", GetEncryptAddr(address).c_str());
    auto classicService = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
    if (IsBtEnabled() && classicService) {
        RawAddress addr(address);
        return classicService->GetAliasName(addr);
    } else {
        HILOGE("BT current state is not enabled");
    }
    return INVALID_NAME;
}

int32_t BluetoothHostServer::SetDeviceAlias(const std::string &address, const std::string &aliasName)
{
    HILOGI("address: %{public}s, aliasName: %{public}s", GetEncryptAddr(address).c_str(), aliasName.c_str());
    auto classicService = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
    if (IsBtEnabled() && classicService) {
        RawAddress addr(address);
        return classicService->SetAliasName(addr, aliasName) ? BT_NO_ERROR : BT_ERR_INVALID_PARAM;
    } else {
        HILOGE("BT current state is not enabled");
    }
    return BT_ERR_INVALID_STATE;
}

int32_t BluetoothHostServer::IsProfileExist(const std::string &profileName, bool &isProfileExist)
{
    sptr<IRemoteObject> profilePtr = GetProfile(profileName);
    if (profilePtr == nullptr) {
        isProfileExist = false;
    } else {
        isProfileExist = true;
    }
    return BT_NO_ERROR;
}

int32_t BluetoothHostServer::GetRemoteDeviceBatteryInfo(const std::string &address,
    BluetoothBatteryInfo &batteryInfo)
{
    return BT_ERR_INTERNAL_ERROR;
}

int32_t BluetoothHostServer::SetRemoteDeviceBatteryInfo(const std::string &address,
    const BluetoothBatteryInfo &batteryInfo)
{
    return NO_ERROR;
}

int32_t BluetoothHostServer::GetPairState(int32_t transport, const std::string &address, int32_t &pairState)
{
    HILOGI("transport: %{public}d, address: %{public}s", transport, GetEncryptAddr(address).c_str());
    auto classicService = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
    auto bleService = IAdapterManager::GetInstance()->GetBleAdapterInterface();
    RawAddress addr(address);
    if ((transport == BT_TRANSPORT_BREDR) && IsBtEnabled() && classicService) {
        pairState = classicService->GetPairState(addr);
    } else if ((transport == BT_TRANSPORT_BLE) && IsBleEnabled() && bleService) {
        pairState = bleService->GetPairState(addr);
    } else {
        HILOGE("transport invalid or BT current state is not enabled!");
    }
    return BT_NO_ERROR;
}

int32_t BluetoothHostServer::StartPair(int32_t transport, const BluetoothRawAddress &bluetoothRawAddress,
    const BluetoothOobData &oobData)
{
    const bool isOob = oobData.HasOobData() || oobData.HasAddressWithType() || oobData.HasConfirmHash() ||
        oobData.HasRandomHash() || oobData.HasDeviceName();
    if (isOob) {
        int32_t permission = PermissionManager::VerifyMultiPermissions(CHECK_PERM(true, {}, {ACCESS_BLUETOOTH}));
        if (permission != NO_ERROR) {
            return permission;
        }
        if (transport != BT_TRANSPORT_BREDR && transport != BT_TRANSPORT_BLE) {
            return BT_ERR_INVALID_PARAM;
        }
        auto raw = oobData.GetAddressWithType();
        auto confirmation = oobData.GetConfirmationHash();
        auto randomizer = oobData.GetRandomizerHash();
        if (!oobData.HasOobData() || raw.size() != 7 || confirmation.size() != 16 ||
            (oobData.HasRandomHash() && randomizer.size() != 16) || raw[6] > 1 ||
            (transport == BT_TRANSPORT_BREDR && raw[6] != 0) ||
            (oobData.GetOobDataType() != OHOS::bluetooth::P192 && oobData.GetOobDataType() != OHOS::bluetooth::P256) ||
            oobData.GetDeviceRole() > OHOS::bluetooth::DEVICE_ROLE_BOTH_PREFER_CENTRAL ||
            (oobData.HasDeviceName() && (oobData.GetDeviceName().empty() || oobData.GetDeviceName().size() > 256))) {
            return BT_ERR_INVALID_PARAM;
        }
        PairingOobData data;
        std::reverse_copy(raw.begin(), raw.begin() + 6, data.peer.begin());
        data.addressType = raw[6];
        data.curve = oobData.GetOobDataType();
        data.hasRandomizer = oobData.HasRandomHash();
        std::copy(confirmation.begin(), confirmation.end(), data.confirmation.begin());
        if (data.hasRandomizer) {
            std::copy(randomizer.begin(), randomizer.end(), data.randomizer.begin());
        }
        std::string target = bluetoothRawAddress.GetAddress();
        std::transform(target.begin(), target.end(), target.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        // This fixed service has no virtual-to-raw mapping. Reject a mismatch
        // rather than applying one peer's OOB material to another device.
        if (target != RawAddress::ConvertToString(data.peer.data()).GetAddress()) {
            return BT_ERR_INVALID_PARAM;
        }
        if ((transport == BT_TRANSPORT_BREDR && !IsBtEnabled()) ||
            (transport == BT_TRANSPORT_BLE && !IsBleEnabled())) {
            return BT_ERR_INVALID_STATE;
        }
        if (transport == BT_TRANSPORT_BLE && data.curve != OHOS::bluetooth::P256) {
            // LE legacy requires a TK, which this C/R SDK shape cannot carry.
            return BT_ERR_API_NOT_SUPPORT;
        }
        if (!data.hasRandomizer) {
            // This backend cannot validate C without its matching R.
            return BT_ERR_API_NOT_SUPPORT;
        }
        RawAddress device(target);
        auto classic = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
        auto ble = IAdapterManager::GetInstance()->GetBleAdapterInterface();
        if (transport == BT_TRANSPORT_BREDR && IsBtEnabled() && classic) {
            return classic->StartPairOutOfBand(device, data) ? NO_ERROR : BT_ERR_INTERNAL_ERROR;
        }
        if (transport == BT_TRANSPORT_BLE && IsBleEnabled() && ble) {
            return ble->StartPairOutOfBand(device, data) ? NO_ERROR : BT_ERR_INTERNAL_ERROR;
        }
        return BT_ERR_INVALID_STATE;
    }
    std::string address = bluetoothRawAddress.GetAddress();
    HILOGI("transport: %{public}d, address: %{public}s", transport, GetEncryptAddr(address).c_str());
    auto classicService = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
    auto bleService = IAdapterManager::GetInstance()->GetBleAdapterInterface();
    RawAddress addr(address);
    transport = GetTransportByDeviceType(transport, address);
    if ((transport == BT_TRANSPORT_BREDR) && IsBtEnabled() && classicService) {
        if (classicService->StartPair(addr)) {
            return NO_ERROR;
        }
    } else if ((transport == BT_TRANSPORT_BLE) && IsBleEnabled() && bleService) {
        if (bleService->StartPair(addr)) {
            return NO_ERROR;
        }
    } else {
        HILOGE("transport invalid or BT current state is not enabled!");
        return BT_ERR_INVALID_STATE;
    }
    return BT_ERR_INTERNAL_ERROR;
}

bool BluetoothHostServer::CancelPairing(int32_t transport, const std::string &address)
{
    HILOGI("transport: %{public}d, address: %{public}s", transport, GetEncryptAddr(address).c_str());
    auto classicService = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
    auto bleService = IAdapterManager::GetInstance()->GetBleAdapterInterface();
    RawAddress addr(address);
    transport = GetTransportByDeviceType(transport, address);
    if ((transport == BT_TRANSPORT_BREDR) && IsBtEnabled() && classicService) {
        return classicService->CancelPairing(addr);
    } else if ((transport == BT_TRANSPORT_BLE) && IsBleEnabled() && bleService) {
        return bleService->CancelPairing(addr);
    } else {
        HILOGE("transport invalid or BT current state is not enabled!");
    }
    return false;
}

bool BluetoothHostServer::IsBondedFromLocal(int32_t transport, const std::string &address)
{
    HILOGI("transport: %{public}d, address: %{public}s", transport, GetEncryptAddr(address).c_str());
    auto classicService = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
    auto bleService = IAdapterManager::GetInstance()->GetBleAdapterInterface();
    RawAddress addr(address);
    if ((transport == BT_TRANSPORT_BREDR) && IsBtEnabled() && classicService) {
        return classicService->IsBondedFromLocal(addr);
    } else if ((transport == BT_TRANSPORT_BLE) && IsBleEnabled() && bleService) {
        return bleService->IsBondedFromLocal(addr);
    } else {
        HILOGE("transport invalid or BT current state is not enabled!");
    }
    return false;
}

bool BluetoothHostServer::IsAclConnected(int32_t transport, const std::string &address)
{
    HILOGI("transport: %{public}d, address: %{public}s", transport, GetEncryptAddr(address).c_str());
    auto classicService = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
    auto bleService = IAdapterManager::GetInstance()->GetBleAdapterInterface();
    RawAddress addr(address);
    if ((transport == BT_TRANSPORT_BREDR) && IsBtEnabled() && classicService) {
        return classicService->IsAclConnected(addr);
    } else if ((transport == BT_TRANSPORT_BLE) && IsBleEnabled() && bleService) {
        return bleService->IsAclConnected(addr);
    } else {
        HILOGE("transport invalid or BT current state is not enabled!");
    }
    return false;
}

bool BluetoothHostServer::IsAclEncrypted(int32_t transport, const std::string &address)
{
    HILOGI("transport: %{public}d, address: %{public}s", transport, GetEncryptAddr(address).c_str());
    auto classicService = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
    auto bleService = IAdapterManager::GetInstance()->GetBleAdapterInterface();
    RawAddress addr(address);
    if ((transport == BT_TRANSPORT_BREDR) && IsBtEnabled() && classicService) {
        return classicService->IsAclEncrypted(addr);
    } else if ((transport == BT_TRANSPORT_BLE) && IsBleEnabled() && bleService) {
        return bleService->IsAclEncrypted(addr);
    } else {
        HILOGE("transport invalid or BT current state is not enabled!");
    }
    return false;
}

int32_t BluetoothHostServer::GetDeviceClass(const std::string &address, int32_t &cod)
{
    HILOGI("address: %{public}s", GetEncryptAddr(address).c_str());
    auto classicService = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
    if (IsBtEnabled() && classicService) {
        RawAddress addr(address);
        cod = classicService->GetDeviceClass(addr);
    } else {
        HILOGE("BT current state is not enabled!");
        return BT_ERR_INVALID_STATE;
    }
    return NO_ERROR;
}

int32_t BluetoothHostServer::GetDeviceUuids(const std::string &address, std::vector<std::string> &uuids)
{
    std::vector<bluetooth::Uuid> parcelUuids;
    RawAddress addr(address);
    if (!IsBtEnabled()) {
        HILOGE("BT current state is not enabled");
        return BT_ERR_INVALID_STATE;
    }

    auto classicService = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
    if (classicService) {
        parcelUuids = classicService->GetDeviceUuids(addr);
    }
    for (auto Uuid : parcelUuids) {
        uuids.push_back(Uuid.ToString());
    }
    return NO_ERROR;
}

int32_t BluetoothHostServer::GetConnectedBLEDevices(int32_t bleProfile, std::vector<std::string> &ConnectedDevices)
{
    return NO_ERROR;
}

int32_t BluetoothHostServer::GetLocalProfileUuids(std::vector<std::string> &uuids)
{
    return NO_ERROR;
}

int32_t BluetoothHostServer::SetDevicePin(const std::string &address, const std::string &pin)
{
    HILOGI("address: %{public}s, pin: %{public}s", GetEncryptAddr(address).c_str(), pin.c_str());
    auto classicService = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
    if (IsBtEnabled() && classicService) {
        RawAddress addr(address);
        if (classicService->SetDevicePin(addr, pin)) {
            return NO_ERROR;
        }
    } else {
        HILOGE("BT current state is not enabled!");
        return BT_ERR_INVALID_STATE;
    }
    return BT_ERR_INTERNAL_ERROR;
}

int32_t BluetoothHostServer::SetDevicePairingConfirmation(int32_t transport, const std::string &address, bool accept)
{
    HILOGI("transport: %{public}d, address: %{public}s, accept: %{public}d",
        transport, GetEncryptAddr(address).c_str(), accept);
    auto classicService = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
    auto bleService = IAdapterManager::GetInstance()->GetBleAdapterInterface();
    RawAddress addr(address);
    transport = GetTransportByDeviceType(transport, address);
    if ((transport == BT_TRANSPORT_BREDR) && IsBtEnabled() && classicService) {
        if (classicService->SetDevicePairingConfirmation(addr, accept)) {
            return NO_ERROR;
        }
    } else if ((transport == BT_TRANSPORT_BLE) && IsBleEnabled() && bleService) {
        if (bleService->SetDevicePairingConfirmation(addr, accept)) {
            return NO_ERROR;
        }
    } else {
        HILOGE("transport invalid or BT current state is not enabled!");
        return BT_ERR_INVALID_STATE;
    }
    return BT_ERR_INTERNAL_ERROR;
}

bool BluetoothHostServer::SetDevicePasskey(int32_t transport, const std::string &address, int32_t passkey, bool accept)
{
    HILOGI("transport: %{public}d, address: %{public}s, passkey: %{public}d, accept: %{public}d",
        transport, GetEncryptAddr(address).c_str(), passkey, accept);
    auto classicService = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
    auto bleService = IAdapterManager::GetInstance()->GetBleAdapterInterface();
    RawAddress addr(address);
    if ((transport == BT_TRANSPORT_BREDR) && IsBtEnabled() && classicService) {
        return classicService->SetDevicePasskey(addr, passkey, accept);
    } else if ((transport == BT_TRANSPORT_BLE) && IsBleEnabled() && bleService) {
        return bleService->SetDevicePasskey(addr, passkey, accept);
    } else {
        HILOGE("transport invalid or BT current state is not enabled!");
    }
    return false;
}

bool BluetoothHostServer::PairRequestReply(int32_t transport, const std::string &address, bool accept)
{
    HILOGI("transport: %{public}d, address: %{public}s, accept: %{public}d",
        transport, GetEncryptAddr(address).c_str(), accept);
    auto classicService = IAdapterManager::GetInstance()->GetClassicAdapterInterface();
    auto bleService = IAdapterManager::GetInstance()->GetBleAdapterInterface();
    RawAddress addr(address);
    if ((transport == BT_TRANSPORT_BREDR) && IsBtEnabled() && classicService) {
        return classicService->PairRequestReply(addr, accept);
    } else if ((transport == BT_TRANSPORT_BLE) && IsBleEnabled() && bleService) {
        return bleService->PairRequestReply(addr, accept);
    } else {
        HILOGE("transport invalid or BT current state is not enabled!");
    }
    return false;
}

bool BluetoothHostServer::ReadRemoteRssiValue(const std::string &address)
{
    HILOGI("address: %{public}s", GetEncryptAddr(address).c_str());
    auto bleService = IAdapterManager::GetInstance()->GetBleAdapterInterface();
    if (IsBleEnabled() && bleService) {
        RawAddress addr(address);
        return bleService->ReadRemoteRssiValue(addr);
    } else {
        HILOGE("BT current state is not enabled!");
    }
    return false;
}

void BluetoothHostServer::RegisterRemoteDeviceObserver(const sptr<IBluetoothRemoteDeviceObserver> &observer)
{
    HILOGI("Enter!");
    if (observer == nullptr) {
        HILOGE("observer is nullptr!");
        return;
    }
    pimpl->remoteObserversToken_.EnsureInsert(observer->AsObject(), IPCSkeleton::GetCallingTokenID());
    pimpl->remoteObserversPid_.EnsureInsert(observer->AsObject(), IPCSkeleton::GetCallingUid());
    auto func = std::bind(&BluetoothHostServer::DeregisterRemoteDeviceObserver,
        this, std::placeholders::_1);
    pimpl->remoteObservers_.Register(observer, func);
    std::lock_guard<std::mutex> lock(pimpl->remoteDeviceObserversMutex);
    pimpl->remoteDeviceObservers_.push_back(observer);
}

void BluetoothHostServer::DeregisterRemoteDeviceObserver(const sptr<IBluetoothRemoteDeviceObserver> &observer)
{
    HILOGI("Enter!");
    if (observer == nullptr || pimpl == nullptr) {
        HILOGE("observer is nullptr!");
        return;
    }
    {
        std::lock_guard<std::mutex> lock(pimpl->remoteDeviceObserversMutex);
        for (auto iter = pimpl->remoteDeviceObservers_.begin(); iter != pimpl->remoteDeviceObservers_.end(); ++iter) {
            if ((*iter)->AsObject() == observer->AsObject()) {
                pimpl->remoteObservers_.Deregister(*iter);
                pimpl->remoteDeviceObservers_.erase(iter);
                break;
            }
        }
    }
    pimpl->remoteObserversToken_.Erase(observer->AsObject());
    pimpl->remoteObserversPid_.Erase(observer->AsObject());
}

bool BluetoothHostServer::IsBtEnabled()
{
    int32_t state = static_cast<int32_t>(BluetoothState::STATE_OFF);
    GetBtState(state);
    bool isEnabled = (state == static_cast<int32_t>(BluetoothState::STATE_ON)) ? true : false;
    HILOGI("%{public}s", isEnabled ? "true" : "false");
    return isEnabled;
}

void BluetoothHostServer::RegisterBleAdapterObserver(const sptr<IBluetoothHostObserver> &observer)
{
    HILOGI("start.");
    if (observer == nullptr) {
        HILOGE("observer is nullptr!");
        return;
    }
    pimpl->bleObserversToken_.EnsureInsert(observer->AsObject(), IPCSkeleton::GetCallingTokenID());
    pimpl->bleObserversPid_.EnsureInsert(observer->AsObject(), IPCSkeleton::GetCallingUid());
    auto func = std::bind(&BluetoothHostServer::DeregisterBleAdapterObserver, this, std::placeholders::_1);
    pimpl->bleObservers_.Register(observer, func);
    std::lock_guard<std::mutex> lock(pimpl->bleAdapterObserversMutex);
    pimpl->bleAdapterObservers_.push_back(observer);
}

void BluetoothHostServer::DeregisterBleAdapterObserver(const sptr<IBluetoothHostObserver> &observer)
{
    HILOGI("start.");
    if (observer == nullptr || pimpl == nullptr) {
        HILOGE("observer is nullptr!");
        return;
    }
    {
        std::lock_guard<std::mutex> lock(pimpl->bleAdapterObserversMutex);
        for (auto iter = pimpl->bleAdapterObservers_.begin(); iter != pimpl->bleAdapterObservers_.end(); ++iter) {
            if ((*iter)->AsObject() == observer->AsObject()) {
                pimpl->bleObservers_.Deregister(*iter);
                pimpl->bleAdapterObservers_.erase(iter);
                break;
            }
        }
    }
    pimpl->bleObserversToken_.Erase(observer->AsObject());
    pimpl->bleObserversPid_.Erase(observer->AsObject());
}

void BluetoothHostServer::RegisterBlePeripheralCallback(const sptr<IBluetoothBlePeripheralObserver> &observer)
{
    HILOGI("start.");
    if (observer == nullptr) {
        HILOGE("observer is nullptr!");
        return;
    }
    pimpl->bleRemoteObserversToken_.EnsureInsert(observer->AsObject(), IPCSkeleton::GetCallingTokenID());
    auto func = std::bind(&BluetoothHostServer::DeregisterBlePeripheralCallback, this, std::placeholders::_1);
    pimpl->bleRemoteObservers_.Register(observer, func);
    std::lock_guard<std::mutex> lock(pimpl->blePeripheralObserversMutex);
    pimpl->blePeripheralObservers_.push_back(observer);
}

void BluetoothHostServer::DeregisterBlePeripheralCallback(const sptr<IBluetoothBlePeripheralObserver> &observer)
{
    HILOGI("start.");
    if (observer == nullptr) {
        HILOGE("observer is nullptr!");
        return;
    }
    {
        std::lock_guard<std::mutex> lock(pimpl->blePeripheralObserversMutex);
        for (auto iter = pimpl->blePeripheralObservers_.begin(); iter != pimpl->blePeripheralObservers_.end(); ++iter) {
            if ((*iter)->AsObject() == observer->AsObject()) {
                if (pimpl != nullptr) {
                    pimpl->bleRemoteObservers_.Deregister(*iter);
                    pimpl->blePeripheralObservers_.erase(iter);
                    break;
                }
            }
        }
    }
    pimpl->bleRemoteObserversToken_.Erase(observer->AsObject());
}

int32_t BluetoothHostServer::Dump(int32_t fd, const std::vector<std::u16string> &args)
{
    HILOGI("fd: %{public}d", fd);
    std::vector<std::string> argsInStr8;
    std::transform(args.begin(), args.end(), std::back_inserter(argsInStr8), [](const std::u16string &arg) {
        return Str16ToStr8(arg);
    });

    std::string result;
    BluetoothHostDumper::BluetoothDump(argsInStr8, result);

    if (!SaveStringToFd(fd, result)) {
        HILOGE("bluetooth dump save string to fd failed!");
        return ERR_INVALID_OPERATION;
    }
    return ERR_OK;
}

int32_t BluetoothHostServer::SetFastScan(bool isEnable)
{
    return NO_ERROR;
}

int32_t BluetoothHostServer::GetRandomAddress(const std::string &realAddr, std::string &randomAddr, uint64_t tokenId)
{
    return BT_ERR_API_NOT_SUPPORT;
}

int32_t BluetoothHostServer::GetRealAddress(const std::string &randomAddr, std::string &realAddr)
{
    return BT_ERR_API_NOT_SUPPORT;
}

int32_t BluetoothHostServer::SyncRandomAddress(const std::string &realAddr, const std::string &randomAddr)
{
    return NO_ERROR;
}

int32_t BluetoothHostServer::StartCrediblePair(int32_t transport, const std::string &address)
{
    return NO_ERROR;
}

int32_t BluetoothHostServer::SatelliteControl(int type, int state)
{
    return BT_ERR_API_NOT_SUPPORT;
}

int32_t BluetoothHostServer::ConnectAllowedProfiles(const std::string &address)
{
    return BT_ERR_API_NOT_SUPPORT;
}

int32_t BluetoothHostServer::DisconnectAllowedProfiles(const std::string &address)
{
    return BT_ERR_API_NOT_SUPPORT;
}

int32_t BluetoothHostServer::SetDeviceCustomType(const std::string &address, int32_t deviceType)
{
    return BT_ERR_API_NOT_SUPPORT;
}

int32_t BluetoothHostServer::GetRemoteDeviceInfo(const std::string &address,
    std::shared_ptr<BluetoothRemoteDeviceInfo> &deviceInfo, int type)
{
    return BT_ERR_API_NOT_SUPPORT;
}

void BluetoothHostServer::RegisterBtResourceManagerObserver(const sptr<IBluetoothResourceManagerObserver> &observer)
{}

void BluetoothHostServer::DeregisterBtResourceManagerObserver(const sptr<IBluetoothResourceManagerObserver> &observer)
{}

void BluetoothHostServer::UpdateVirtualDevice(int32_t action, const std::string &address)
{}

int32_t BluetoothHostServer::IsSupportVirtualAutoConnect(const std::string &address, bool &outSupport)
{
    outSupport = false;
    return BT_ERR_API_NOT_SUPPORT;
}

int32_t BluetoothHostServer::SetVirtualAutoConnectType(const std::string &address, int connType, int businessType)
{
    return BT_ERR_API_NOT_SUPPORT;
}

int32_t BluetoothHostServer::SetFastScanLevel(int level)
{
    return BT_ERR_API_NOT_SUPPORT;
}

int32_t BluetoothHostServer::EnableBluetoothToRestrictMode(const std::string &callingName)
{
    (void)(callingName);
    return BT_ERR_API_NOT_SUPPORT;
}

int32_t BluetoothHostServer::ControlDeviceAction(const std::string &deviceId, uint32_t controlType,
    uint32_t controlTypeVal, uint32_t controlObject)
{
    return BT_ERR_API_NOT_SUPPORT;
}

int32_t BluetoothHostServer::GetLastConnectionTime(const std::string &address, int64_t &connectionTime)
{
    return BT_ERR_API_NOT_SUPPORT;
}

int32_t BluetoothHostServer::UpdateCloudBluetoothDevice(std::vector<BluetoothTrustPairDevice> &cloudDevices)
{
    return BT_ERR_API_NOT_SUPPORT;
}

int32_t BluetoothHostServer::GetCloudBondState(const std::string &address, int32_t &cloudBondState)
{
    return BT_ERR_API_NOT_SUPPORT;
}

int32_t BluetoothHostServer::GetDeviceTransport(const std::string &address, int32_t &transport)
{
    return BT_ERR_API_NOT_SUPPORT;
}

int32_t BluetoothHostServer::UpdateRefusePolicy(const int32_t protocolType,
    const int32_t pid, const int64_t prohibitedSecondsTime)
{
    return BT_ERR_API_NOT_SUPPORT;
}

int32_t BluetoothHostServer::ProcessRandomDeviceIdCommand(
    int32_t command, std::vector<std::string> &deviceIdVec, bool &isValid)
{
    return BT_ERR_API_NOT_SUPPORT;
}

int32_t BluetoothHostServer::GetCarKeyDfxData(std::string &dfxData)
{
    return BT_ERR_API_NOT_SUPPORT;
}

int32_t BluetoothHostServer::SetCarKeyCardData(const std::string &address, int32_t action)
{
    return BT_ERR_API_NOT_SUPPORT;
}

int32_t BluetoothHostServer::NotifyDialogResult(uint32_t dialogType, bool dialogResult)
{
    return BT_ERR_API_NOT_SUPPORT;
}

void BluetoothHostServer::SetCallingPackageName(const std::string &address, const std::string &packageName)
{}

int32_t BluetoothHostServer::StartRemoteSdpSearch(const std::string &address, const std::string &uuid)
{
    return BT_ERR_API_NOT_SUPPORT;
}
int32_t BluetoothHostServer::GetRemoteServices(const std::string &address)
{
    return BT_ERR_API_NOT_SUPPORT;
}

int32_t BluetoothHostServer::GetVirtualAddressByHash(
    int hashAlgorithmType, const std::string &hashValue, std::string &virtualAddress)
{
    return BT_ERR_API_NOT_SUPPORT;
}

int32_t BluetoothHostServer::SetConnectionPriority(const std::string &address, int32_t priority)
{
    return BT_ERR_API_NOT_SUPPORT;
}

int32_t BluetoothHostServer::VerifyMultiPermissions(bool systemHapNeeded,
    const std::set<std::string> &permissions)
{
    return BT_ERR_API_NOT_SUPPORT;
}
}  // namespace Bluetooth
}  // namespace OHOS
