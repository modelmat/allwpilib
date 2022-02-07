// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#include "glass/networktables/NTPhotonCamera.h"
#include "glass/other/PhotonCamera.h"

#include <algorithm>
#include <string_view>
#include <vector>

#include <fmt/format.h>
#include <imgui.h>
#include <ntcore_cpp.h>
#include <wpi/StringExtras.h>

using namespace glass;

// Convert "#RRGGBB" hex color to ImU32 color
static void ConvertColor(std::string_view in, ImU32* out) {
  if (in.size() != 7 || in[0] != '#') {
    return;
  }
  if (auto v = wpi::parse_integer<ImU32>(wpi::drop_front(in), 16)) {
    ImU32 val = v.value();
    *out = IM_COL32((val >> 16) & 0xff, (val >> 8) & 0xff, val & 0xff, 255);
  }
}

namespace {

class NTPhotonCameraViewObjectModel;

class NTPhotonCameraViewGroupImpl final {
 public:
  NTPhotonCameraViewGroupImpl(NT_Inst inst, std::string_view path,
                              std::string_view name)
      : m_inst{inst}, m_path{path}, m_name{name} {}

  const char* GetName() const { return m_name.c_str(); }
  void ForEachObject(
      wpi::function_ref<void(PhotonCameraObjectModel& model)> func);
  void NTUpdate(const nt::EntryNotification& event, std::string_view name);

 protected:
  NT_Inst m_inst;
  std::string m_path;
  std::string m_name;
  std::vector<std::unique_ptr<NTPhotonCameraViewObjectModel>> m_objects;
};

class NTPhotonCameraViewObjectModel final : public PhotonCameraObjectModel {
 public:
  NTPhotonCameraViewObjectModel(NT_Inst inst, std::string_view path,
                                std::string_view name)
      : m_group{inst, path, name},
        m_type{nt::GetEntry(inst, fmt::format("{}/.type", path))},
        m_color{nt::GetEntry(inst, fmt::format("{}/color", path))},
        m_weight{nt::GetEntry(inst, fmt::format("{}/weight", path))},
        m_x{nt::GetEntry(inst, fmt::format("{}/x", path))},
        m_y{nt::GetEntry(inst, fmt::format("{}/y", path))} {}

  const char* GetName() const final { return m_group.GetName(); }
  void ForEachObject(
      wpi::function_ref<void(PhotonCameraObjectModel& model)> func) final {
    m_group.ForEachObject(func);
  }

  const char* GetType() const final { return m_typeValue.c_str(); }
  ImU32 GetColor() const final { return m_colorValue; }
  double GetWeight() const final { return m_weightValue; }
  double GetX() const final { return m_xValue; }
  double GetY() const final { return m_yValue; }

  bool NTUpdate(const nt::EntryNotification& event, std::string_view childName);

 private:
  NTPhotonCameraViewGroupImpl m_group;

  NT_Entry m_type;
  NT_Entry m_color;
  NT_Entry m_weight;
  NT_Entry m_x;
  NT_Entry m_y;

  std::string m_typeValue;
  ImU32 m_colorValue = IM_COL32_WHITE;
  double m_weightValue = 1.0;
  double m_xValue = 0.0;
  double m_yValue = 0.0;
};

}  // namespace

void NTPhotonCameraViewGroupImpl::ForEachObject(
    wpi::function_ref<void(PhotonCameraObjectModel& model)> func) {
  for (auto&& obj : m_objects) {
    func(*obj);
  }
}

void NTPhotonCameraViewGroupImpl::NTUpdate(const nt::EntryNotification& event,
                                           std::string_view name) {
  if (name.empty()) {
    return;
  }
  std::string_view childName;
  std::tie(name, childName) = wpi::split(name, '/');
  if (childName.empty()) {
    return;
  }

  auto it = std::lower_bound(
      m_objects.begin(), m_objects.end(), name,
      [](const auto& e, std::string_view name) { return e->GetName() < name; });
  bool match = it != m_objects.end() && (*it)->GetName() == name;

  if (event.flags & NT_NOTIFY_NEW) {
    if (!match) {
      it = m_objects.emplace(
          it, std::make_unique<NTPhotonCameraViewObjectModel>(
                  m_inst, fmt::format("{}/{}", m_path, name), name));
      match = true;
    }
  }
  if (match) {
    if ((*it)->NTUpdate(event, childName)) {
      m_objects.erase(it);
    }
  }
}

bool NTPhotonCameraViewObjectModel::NTUpdate(const nt::EntryNotification& event,
                                             std::string_view childName) {
  if (event.entry == m_type) {
    if ((event.flags & NT_NOTIFY_DELETE) != 0) {
      return true;
    }
    if (event.value && event.value->IsString()) {
      m_typeValue = event.value->GetString();
    }
  } else if (event.entry == m_color) {
    if (event.value && event.value->IsString()) {
      ConvertColor(event.value->GetString(), &m_colorValue);
    }
  } else if (event.entry == m_weight) {
    if (event.value && event.value->IsDouble()) {
      m_weightValue = event.value->GetDouble();
    }
  } else if (event.entry == m_x) {
    if (event.value && event.value->IsDouble()) {
      m_xValue = event.value->GetDouble();
    }
  } else if (event.entry == m_y) {
    if (event.value && event.value->IsDouble()) {
      m_yValue = event.value->GetDouble();
    }
  } else {
    m_group.NTUpdate(event, childName);
  }
  return false;
}

class NTPhotonCameraViewModel::RootModel final : public PhotonCameraRootModel {
 public:
  RootModel(NT_Inst inst, std::string_view path, std::string_view name)
      : m_group{inst, path, name},
        m_x{nt::GetEntry(inst, fmt::format("{}/x", path))},
        m_y{nt::GetEntry(inst, fmt::format("{}/y", path))} {}

  const char* GetName() const final { return m_group.GetName(); }
  void ForEachObject(
      wpi::function_ref<void(PhotonCameraObjectModel& model)> func) final {
    m_group.ForEachObject(func);
  }

  bool NTUpdate(const nt::EntryNotification& event, std::string_view childName);

  frc::Translation2d GetPosition() const override { return m_pos; };

 private:
  NTPhotonCameraViewGroupImpl m_group;
  NT_Entry m_x;
  NT_Entry m_y;
  frc::Translation2d m_pos;
};

bool NTPhotonCameraViewModel::RootModel::NTUpdate(
    const nt::EntryNotification& event, std::string_view childName) {
  if ((event.flags & NT_NOTIFY_DELETE) != 0 &&
      (event.entry == m_x || event.entry == m_y)) {
    return true;
  } else if (event.entry == m_x) {
    if (event.value && event.value->IsDouble()) {
      m_pos = frc::Translation2d{units::meter_t{event.value->GetDouble()},
                                 m_pos.Y()};
    }
  } else if (event.entry == m_y) {
    if (event.value && event.value->IsDouble()) {
      m_pos = frc::Translation2d{m_pos.X(),
                                 units::meter_t{event.value->GetDouble()}};
    }
  } else {
    m_group.NTUpdate(event, childName);
  }
  return false;
}

NTPhotonCameraViewModel::NTPhotonCameraViewModel(std::string_view path)
    : NTPhotonCameraViewModel{nt::GetDefaultInstance(), path} {}

NTPhotonCameraViewModel::NTPhotonCameraViewModel(NT_Inst inst,
                                                 std::string_view path)
    : m_nt{inst},
      m_path{fmt::format("{}/", path)},
      m_name{m_nt.GetEntry(fmt::format("{}/.name", path))},
      m_dimensions{m_nt.GetEntry(fmt::format("{}/dims", path))},
      m_bgColor{m_nt.GetEntry(fmt::format("{}/backgroundColor", path))},
      m_dimensionsValue{1_m, 1_m} {
  m_nt.AddListener(m_path, NT_NOTIFY_LOCAL | NT_NOTIFY_NEW | NT_NOTIFY_DELETE |
                               NT_NOTIFY_UPDATE | NT_NOTIFY_IMMEDIATE);
}

NTPhotonCameraViewModel::~NTPhotonCameraViewModel() = default;

void NTPhotonCameraViewModel::Update() {
  for (auto&& event : m_nt.PollListener()) {
    // .name
    if (event.entry == m_name) {
      if (event.value && event.value->IsString()) {
        m_nameValue = event.value->GetString();
      }
      continue;
    }

    // dims
    if (event.entry == m_dimensions) {
      if (event.value && event.value->IsDoubleArray()) {
        auto arr = event.value->GetDoubleArray();
        if (arr.size() == 2) {
          m_dimensionsValue = frc::Translation2d{units::meter_t{arr[0]},
                                                 units::meter_t{arr[1]}};
        }
      }
    }

    // backgroundColor
    if (event.entry == m_bgColor) {
      if (event.value && event.value->IsString()) {
        ConvertColor(event.value->GetString(), &m_bgColorValue);
      }
    }

    std::string_view name = event.name;
    if (wpi::starts_with(name, m_path)) {
      name.remove_prefix(m_path.size());
      if (name.empty() || name[0] == '.') {
        continue;
      }
      std::string_view childName;
      std::tie(name, childName) = wpi::split(name, '/');
      if (childName.empty()) {
        continue;
      }

      auto it = std::lower_bound(m_roots.begin(), m_roots.end(), name,
                                 [](const auto& e, std::string_view name) {
                                   return e->GetName() < name;
                                 });
      bool match = it != m_roots.end() && (*it)->GetName() == name;

      if (event.flags & NT_NOTIFY_NEW) {
        if (!match) {
          it = m_roots.emplace(
              it,
              std::make_unique<RootModel>(
                  m_nt.GetInstance(), fmt::format("{}{}", m_path, name), name));
          match = true;
        }
      }
      if (match) {
        if ((*it)->NTUpdate(event, childName)) {
          m_roots.erase(it);
        }
      }
    }
  }
}

bool NTPhotonCameraViewModel::Exists() {
  return m_nt.IsConnected() && nt::GetEntryType(m_name) != NT_UNASSIGNED;
}

bool NTPhotonCameraViewModel::IsReadOnly() {
  return false;
}

void NTPhotonCameraViewModel::ForEachRoot(
    wpi::function_ref<void(PhotonCameraRootModel& model)> func) {
  for (auto&& obj : m_roots) {
    func(*obj);
  }
}
