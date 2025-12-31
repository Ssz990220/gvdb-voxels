//-----------------------------------------------------------------------------
// NVIDIA(R) GVDB VOXELS
// Copyright 2017 NVIDIA Corporation
// SPDX-License-Identifier: Apache-2.0
//
// gBoolean - Boolean Operations Demo
// Demonstrates Union, Intersection, and Difference operations between GVDB volumes
//-----------------------------------------------------------------------------

// GVDB library
#include "gvdb.h"
using namespace nvdb;

// Sample utils
#include <GL/glew.h>

#include "main.h"    // window system
#include "nv_gui.h"  // gui system

// Two GVDB volumes for boolean operations
VolumeGVDB gvdbA;  // Volume A (e.g., bunny)
VolumeGVDB gvdbB;  // Volume B (e.g., cube)
VolumeGVDB gvdbResult;  // Result volume

// Boolean operation types
enum BooleanOp {
  BOOL_NONE = 0,
  BOOL_UNION = 1,
  BOOL_INTERSECTION = 2,
  BOOL_DIFFERENCE_AB = 3,  // A - B
  BOOL_DIFFERENCE_BA = 4,  // B - A
  BOOL_SHOW_A = 5,
  BOOL_SHOW_B = 6
};

class Sample : public NVPWindow {
 public:
  virtual bool init();
  virtual void display();
  virtual void reshape(int w, int h);
  virtual void motion(int x, int y, int dx, int dy);
  virtual void keyboardchar(unsigned char key, int mods, int x, int y);
  virtual void mouse(NVPWindow::MouseButton button, NVPWindow::ButtonAction state, int mods, int x, int y);

  void voxelizeModels();
  void performBooleanOp();
  void draw_topology(VolumeGVDB* gvdb);
  void start_guis(int w, int h);

  int gl_screen_tex;
  int mouse_down;

  Vector3DF m_pivot;
  float m_voxel_size;

  bool m_show_topo;
  int m_shade_style;
  int m_bool_op;  // Current boolean operation

  // Transform for model B (to offset it from A)
  Vector3DF m_offsetB;
};

Sample sample_obj;

void handle_gui(int gui, float val) {
  switch (gui) {
    case 2: {  // Boolean operation changed
      sample_obj.performBooleanOp();
    } break;
  }
}

void Sample::start_guis(int w, int h) {
  clearGuis();
  setview2D(w, h);
  guiSetCallback(handle_gui);
  addGui(10, h - 30, 130, 20, "Topology", GUI_CHECK, GUI_BOOL, &m_show_topo, 0.f, 1.f);
  addGui(150, h - 30, 130, 20, "Shading", GUI_COMBO, GUI_INT, &m_shade_style, 0.f, 5.f);
  addItem("Voxel");
  addItem("Surface");
  addItem("Section");
  addItem("Volume");
  addGui(300, h - 30, 200, 20, "Boolean Op", GUI_COMBO, GUI_INT, &m_bool_op, 0.f, 7.f);
  addItem("None (A)");
  addItem("Union (A+B)");
  addItem("Intersect (A&B)");
  addItem("Diff (A-B)");
  addItem("Diff (B-A)");
  addItem("Show A only");
  addItem("Show B only");
}

void Sample::voxelizeModels() {
  // Voxel size - controls resolution
  m_voxel_size = 0.5f;
  float part_size = 100.0f;

  // Create transform for model A (bunny)
  // Bunny model is roughly unit sized, scale it up
  Matrix4F xformA, m;
  xformA.Identity();
  m.Scale(part_size, part_size, part_size);
  xformA *= m;
  m.Scale(1 / m_voxel_size, 1 / m_voxel_size, 1 / m_voxel_size);
  xformA *= m;
  m.Translate(0.5f, 0.45f, 0.5f);  // Center the bunny
  xformA *= m;

  // Create transform for model B (cube) - scale to match bunny size
  // The cube.obj is larger than bunny.obj, so scale it down
  Matrix4F xformB;
  xformB.Identity();
  m.Scale(part_size * 0.25f, part_size * 0.25f, part_size * 0.25f);  // Scale down cube to match bunny
  xformB *= m;
  m.Scale(1 / m_voxel_size, 1 / m_voxel_size, 1 / m_voxel_size);
  xformB *= m;
  m.Translate(m_offsetB.x, m_offsetB.y, m_offsetB.z);  // Offset to partially overlap
  xformB *= m;

  // Setup volume A
  printf("Voxelizing model A (bunny)...\n");
  gvdbA.DestroyChannels();
  gvdbA.AddChannel(0, T_FLOAT, 1);
  gvdbA.SetTransform(Vector3DF(0, 0, 0), Vector3DF(m_voxel_size, m_voxel_size, m_voxel_size),
                     Vector3DF(0, 0, 0), Vector3DF(0, 0, 0));

  Model* modelA = gvdbA.getScene()->getModel(0);
  gvdbA.SolidVoxelize(0, modelA, &xformA, 1.0, 0.5);
  gvdbA.Measure(true);

  // Setup volume B
  printf("Voxelizing model B (cube)...\n");
  gvdbB.DestroyChannels();
  gvdbB.AddChannel(0, T_FLOAT, 1);
  gvdbB.SetTransform(Vector3DF(0, 0, 0), Vector3DF(m_voxel_size, m_voxel_size, m_voxel_size),
                     Vector3DF(0, 0, 0), Vector3DF(0, 0, 0));

  Model* modelB = gvdbB.getScene()->getModel(0);
  gvdbB.SolidVoxelize(0, modelB, &xformB, 1.0, 0.5);
  gvdbB.Measure(true);

  // Initial boolean operation
  performBooleanOp();
}

void Sample::performBooleanOp() {
  printf("Performing boolean operation: %d\n", m_bool_op);

  // Setup result volume with same config as A
  gvdbResult.DestroyChannels();
  gvdbResult.AddChannel(0, T_FLOAT, 1);
  gvdbResult.SetTransform(Vector3DF(0, 0, 0), Vector3DF(m_voxel_size, m_voxel_size, m_voxel_size),
                          Vector3DF(0, 0, 0), Vector3DF(0, 0, 0));

  switch (m_bool_op) {
    case BOOL_NONE:
    case BOOL_SHOW_A:
      // Just copy A's topology and data
      gvdbResult.ActivateBricksFrom(&gvdbA);
      gvdbResult.FinishTopology();
      gvdbResult.UpdateAtlas();
      gvdbResult.CopyChannel(0, 0);  // This won't work cross-volume, we need manual copy
      // For now, just show A directly
      break;

    case BOOL_SHOW_B:
      // Show B
      gvdbResult.ActivateBricksFrom(&gvdbB);
      gvdbResult.FinishTopology();
      gvdbResult.UpdateAtlas();
      break;

    case BOOL_UNION:
      // Copy A first
      gvdbResult.ActivateBricksFrom(&gvdbA);
      gvdbResult.FinishTopology();
      gvdbResult.UpdateAtlas();
      // Perform union: Result = A union B
      gvdbResult.BooleanUnion(0, 0, &gvdbB, 0, true);
      break;

    case BOOL_INTERSECTION:
      // Copy A first
      gvdbResult.ActivateBricksFrom(&gvdbA);
      gvdbResult.FinishTopology();
      gvdbResult.UpdateAtlas();
      // Perform intersection: Result = A intersect B
      gvdbResult.BooleanIntersection(0, 0, &gvdbB, 0, true);
      break;

    case BOOL_DIFFERENCE_AB:
      // Copy A first
      gvdbResult.ActivateBricksFrom(&gvdbA);
      gvdbResult.FinishTopology();
      gvdbResult.UpdateAtlas();
      // Perform difference: Result = A - B
      gvdbResult.BooleanDifference(0, 0, &gvdbB, 0, true);
      break;

    case BOOL_DIFFERENCE_BA:
      // Copy B first
      gvdbResult.ActivateBricksFrom(&gvdbB);
      gvdbResult.FinishTopology();
      gvdbResult.UpdateAtlas();
      // Perform difference: Result = B - A
      gvdbResult.BooleanDifference(0, 0, &gvdbA, 0, true);
      break;
  }

  gvdbResult.Measure(true);
}

bool Sample::init() {
  int w = getWidth(), h = getHeight();
  mouse_down = -1;
  gl_screen_tex = -1;
  m_show_topo = false;
  m_shade_style = 1;  // Surface shading
  m_bool_op = BOOL_UNION;  // Start with union
  m_pivot.Set(0.5f, 0.5f, 0.5f);
  m_offsetB.Set(0.6f, 0.5f, 0.6f);  // Offset cube to partially overlap with bunny
  srand(6572);

  init2D("arial");
  setview2D(w, h);

  // Initialize GVDB volume A
  printf("Initializing GVDB volume A...\n");
  gvdbA.SetDebug(true);
  gvdbA.SetVerbose(true);
  gvdbA.SetCudaDevice(GVDB_DEV_FIRST);
  gvdbA.Initialize();
  gvdbA.StartRasterGL();
  gvdbA.AddPath("../source/shared_assets/");
  gvdbA.AddPath("../../source/shared_assets/");
  gvdbA.AddPath(ASSET_PATH);

  // Initialize GVDB volume B (shares CUDA context)
  printf("Initializing GVDB volume B...\n");
  gvdbB.SetDebug(true);
  gvdbB.SetVerbose(true);
  gvdbB.SetCudaDevice(GVDB_DEV_CURRENT);  // Use same device
  gvdbB.Initialize();
  gvdbB.StartRasterGL();
  gvdbB.AddPath("../source/shared_assets/");
  gvdbB.AddPath("../../source/shared_assets/");
  gvdbB.AddPath(ASSET_PATH);

  // Initialize result volume
  printf("Initializing result volume...\n");
  gvdbResult.SetDebug(true);
  gvdbResult.SetVerbose(true);
  gvdbResult.SetCudaDevice(GVDB_DEV_CURRENT);
  gvdbResult.Initialize();
  gvdbResult.StartRasterGL();
  gvdbResult.AddPath("../source/shared_assets/");
  gvdbResult.AddPath("../../source/shared_assets/");
  gvdbResult.AddPath(ASSET_PATH);

  // Load polygon models
  printf("Loading polygon models...\n");
  gvdbA.getScene()->AddModel("bunny.obj", 1.0, 0, 0, 0);  // Model A: bunny
  gvdbA.CommitGeometry(0);

  gvdbB.getScene()->AddModel("cube.obj", 1.0, 0, 0, 0);  // Model B: cube
  gvdbB.CommitGeometry(0);

  // Configure GVDB trees
  printf("Configuring GVDB...\n");
  gvdbA.Configure(3, 3, 3, 3, 5);
  gvdbA.SetChannelDefault(16, 16, 1);

  gvdbB.Configure(3, 3, 3, 3, 5);
  gvdbB.SetChannelDefault(16, 16, 1);

  gvdbResult.Configure(3, 3, 3, 3, 5);
  gvdbResult.SetChannelDefault(16, 16, 1);

  // Voxelize models
  voxelizeModels();

  // Set volume rendering params for result
  printf("Setting volume params...\n");
  gvdbResult.getScene()->SetSteps(0.5f, 16.f, 0.5f);
  gvdbResult.getScene()->SetVolumeRange(0.25f, 0.0f, 1.0f);
  gvdbResult.getScene()->SetExtinct(-1.0f, 1.1f, 0.f);
  gvdbResult.getScene()->SetCutoff(0.005f, 0.005f, 0.f);
  gvdbResult.getScene()->SetShadowParams(0, 0, 0);
  gvdbResult.getScene()->LinearTransferFunc(0.0f, 0.5f, Vector4DF(0, 0, 0, 0), Vector4DF(0.8f, 0.5f, 0.2f, 0.5f));
  gvdbResult.getScene()->LinearTransferFunc(0.5f, 1.0f, Vector4DF(0.8f, 0.5f, 0.2f, 0.5f), Vector4DF(1, 0.8f, 0.6f, 0.8f));
  gvdbResult.CommitTransferFunc();
  gvdbResult.getScene()->SetBackgroundClr(0.1f, 0.2f, 0.4f, 1.0f);

  // Create Camera
  Camera3D* cam = new Camera3D;
  cam->setFov(50.0);
  cam->setOrbit(Vector3DF(-45.f, 30.f, 0.f), m_pivot * 100.0f, 300.f, 1.0f);
  gvdbResult.getScene()->SetCamera(cam);

  // Create Light
  Light* lgt = new Light;
  lgt->setOrbit(Vector3DF(299.0f, 57.3f, 0.f), m_pivot * 100.0f * Vector3DF(1.3f, 1.8f, 1.1f), 200.f, 1.0f);
  gvdbResult.getScene()->SetLight(0, lgt);

  // Add render buffer
  printf("Creating screen buffer. %d x %d\n", w, h);
  gvdbResult.AddRenderBuf(0, w, h, 4);

  // Screen texture
  glViewport(0, 0, w, h);
  createScreenQuadGL(&gl_screen_tex, w, h);

  start_guis(w, h);

  printf("\n=== gBoolean Controls ===\n");
  printf("1: Toggle topology view\n");
  printf("2: Cycle shading modes\n");
  printf("3: Cycle boolean operations\n");
  printf("Left mouse: Rotate camera\n");
  printf("Right mouse: Zoom\n");
  printf("Middle mouse: Pan\n");
  printf("========================\n\n");

  return true;
}

void Sample::display() {
  clearScreenGL();

  int sh;
  switch (m_shade_style) {
    case 0: sh = SHADE_VOXEL; break;
    case 1: sh = SHADE_TRILINEAR; break;
    case 2: sh = SHADE_SECTION3D; break;
    case 3: sh = SHADE_VOLUME; break;
    default: sh = SHADE_TRILINEAR; break;
  }

  // Render based on current operation
  VolumeGVDB* renderVol = &gvdbResult;

  // For simple show operations, render the source directly
  if (m_bool_op == BOOL_SHOW_A || m_bool_op == BOOL_NONE) {
    renderVol = &gvdbA;
    // Share camera/light with gvdbA
    gvdbA.getScene()->SetCamera(gvdbResult.getScene()->getCamera());
    gvdbA.getScene()->SetLight(0, gvdbResult.getScene()->getLight());
    gvdbA.getScene()->SetSteps(0.5f, 16.f, 0.5f);
    gvdbA.getScene()->SetVolumeRange(0.25f, 0.0f, 1.0f);
    gvdbA.getScene()->SetBackgroundClr(0.1f, 0.2f, 0.4f, 1.0f);
    gvdbA.AddRenderBuf(0, getWidth(), getHeight(), 4);
  } else if (m_bool_op == BOOL_SHOW_B) {
    renderVol = &gvdbB;
    gvdbB.getScene()->SetCamera(gvdbResult.getScene()->getCamera());
    gvdbB.getScene()->SetLight(0, gvdbResult.getScene()->getLight());
    gvdbB.getScene()->SetSteps(0.5f, 16.f, 0.5f);
    gvdbB.getScene()->SetVolumeRange(0.25f, 0.0f, 1.0f);
    gvdbB.getScene()->SetBackgroundClr(0.1f, 0.2f, 0.4f, 1.0f);
    gvdbB.AddRenderBuf(0, getWidth(), getHeight(), 4);
  }

  renderVol->Render(sh, 0, 0);
  renderVol->ReadRenderTexGL(0, gl_screen_tex);
  renderScreenQuadGL(gl_screen_tex);

  if (m_show_topo) draw_topology(renderVol);

  draw3D();
  drawGui(0);
  draw2D();
}

void Sample::draw_topology(VolumeGVDB* gvdb) {
  start3D(gvdb->getScene()->getCamera());

  for (int lev = 0; lev < 5; lev++) {
    int node_cnt = static_cast<int>(gvdb->getNumNodes(lev));
    const Vector3DF& color = gvdb->getClrDim(lev);
    const Matrix4F& xform = gvdb->getTransform();

    for (int n = 0; n < node_cnt; n++) {
      Node* node = gvdb->getNodeAtLevel(n, lev);
      Vector3DF bmin = gvdb->getWorldMin(node);
      Vector3DF bmax = gvdb->getWorldMax(node);
      drawBox3DXform(bmin, bmax, color, xform);
    }
  }

  end3D();
}

void Sample::motion(int x, int y, int dx, int dy) {
  Camera3D* cam = gvdbResult.getScene()->getCamera();
  Light* lgt = gvdbResult.getScene()->getLight();
  bool shift = (getMods() & NVPWindow::KMOD_SHIFT);

  switch (mouse_down) {
    case NVPWindow::MOUSE_BUTTON_LEFT: {
      Vector3DF angs = (shift ? lgt->getAng() : cam->getAng());
      angs.x += dx * 0.2f;
      angs.y -= dy * 0.2f;
      if (shift)
        lgt->setOrbit(angs, lgt->getToPos(), lgt->getOrbitDist(), lgt->getDolly());
      else
        cam->setOrbit(angs, cam->getToPos(), cam->getOrbitDist(), cam->getDolly());
      postRedisplay();
    } break;

    case NVPWindow::MOUSE_BUTTON_MIDDLE: {
      cam->moveRelative(float(dx) * cam->getOrbitDist() / 1000, float(-dy) * cam->getOrbitDist() / 1000, 0);
      postRedisplay();
    } break;

    case NVPWindow::MOUSE_BUTTON_RIGHT: {
      float dist = (shift ? lgt->getOrbitDist() : cam->getOrbitDist());
      dist -= dy;
      if (shift)
        lgt->setOrbit(lgt->getAng(), lgt->getToPos(), dist, cam->getDolly());
      else
        cam->setOrbit(cam->getAng(), cam->getToPos(), dist, cam->getDolly());
      postRedisplay();
    } break;
  }
}

void Sample::mouse(NVPWindow::MouseButton button, NVPWindow::ButtonAction state, int mods, int x, int y) {
  if (guiHandler(button, state, x, y)) return;
  mouse_down = (state == NVPWindow::BUTTON_PRESS) ? button : -1;
}

void Sample::keyboardchar(unsigned char key, int mods, int x, int y) {
  switch (key) {
    case '1':
      m_show_topo = !m_show_topo;
      break;
    case '2':
      m_shade_style = (m_shade_style + 1) % 4;
      break;
    case '3':
      m_bool_op = (m_bool_op + 1) % 7;
      performBooleanOp();
      break;
  }
  postRedisplay();
}

void Sample::reshape(int w, int h) {
  glViewport(0, 0, w, h);
  createScreenQuadGL(&gl_screen_tex, w, h);
  gvdbResult.ResizeRenderBuf(0, w, h, 4);
  start_guis(w, h);
  postRedisplay();
}

int sample_main(int argc, const char** argv) {
  return sample_obj.run("NVIDIA(R) GVDB Voxels - gBoolean Operations", "boolean", argc, argv, 1024, 768, 4, 5);
}

void sample_print(int argc, char const* argv) {}
