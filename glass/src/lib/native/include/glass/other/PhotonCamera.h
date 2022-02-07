// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#pragma once

#include <frc/geometry/Rotation2d.h>
#include <frc/geometry/Translation2d.h>
#include <imgui.h>
#include <wpi/function_ref.h>

#include "glass/Model.h"
#include "glass/View.h"

namespace glass {

class PhotonCameraObjectModel;

class PhotonCameraObjectGroup {
 public:
  virtual const char* GetName() const = 0;
  virtual void ForEachObject(
      wpi::function_ref<void(PhotonCameraObjectModel& model)> func) = 0;
};

class PhotonCameraObjectModel : public PhotonCameraObjectGroup {
 public:
  virtual const char* GetType() const = 0;
  virtual ImU32 GetColor() const = 0;

  // line accessors
  virtual double GetWeight() const = 0;
  virtual double GetX() const = 0;
  virtual double GetY() const = 0;
};

class PhotonCameraRootModel : public PhotonCameraObjectGroup {
 public:
  virtual frc::Translation2d GetPosition() const = 0;
};

class PhotonCameraModel : public Model {
 public:
  virtual frc::Translation2d GetDimensions() const = 0;
  virtual ImU32 GetBackgroundColor() const = 0;
  virtual void ForEachRoot(
      wpi::function_ref<void(PhotonCameraRootModel& model)> func) = 0;
};

void DisplayPhotonCamera(PhotonCameraModel* model, const ImVec2& contentSize);
void DisplayPhotonCameraSettings(PhotonCameraModel* model);

class PhotonCameraView : public View {
 public:
  explicit PhotonCameraView(PhotonCameraModel* model) : m_model{model} {}

  void Display() override;

 private:
  PhotonCameraModel* m_model;
};

}  // namespace glass
