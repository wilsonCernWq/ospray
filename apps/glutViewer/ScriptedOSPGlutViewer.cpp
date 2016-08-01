﻿// ======================================================================== //
// Copyright 2009-2016 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#include "ScriptedOSPGlutViewer.h"

// MSGViewer definitions //////////////////////////////////////////////////////

namespace ospray {

  using std::cout;
  using std::endl;
  using std::string;
  using std::lock_guard;
  using std::mutex;
  using namespace ospcommon;

  ScriptedOSPGlutViewer::ScriptedOSPGlutViewer(const box3f   &worldBounds,
                                               cpp::Model     model,
                                               cpp::Renderer  renderer,
                                               cpp::Camera    camera,
                                               std::string    scriptFileName)
    : OSPGlutViewer(worldBounds, model, renderer, camera),
      m_scriptHandler(model.handle(), renderer.handle(), camera.handle(), this),
      frameID(0)
  {
    if (!scriptFileName.empty())
      m_scriptHandler.runScriptFromFile(scriptFileName);
  }

  int ScriptedOSPGlutViewer::getFrameID() const {
    return frameID.load();
  }

  void ScriptedOSPGlutViewer::display() {
    if (!m_fb.handle() || !m_renderer.handle()) return;

    // We need to synchronize with the scripting engine so we don't
    // get our scene data trampled on if scripting is running.
    // TODO: Conditionally lock this? How much performance hit do we really get?
    // It's not like the mutex is heavily contested.
    std::lock_guard<std::mutex> lock(m_scriptHandler.m_scriptMutex);

    //{
    // note that the order of 'start' and 'end' here is
    // (intentionally) reversed: due to our asynchrounous rendering
    // you cannot place start() and end() _around_ the renderframe
    // call (which in itself will not do a lot other than triggering
    // work), but the average time between the two calls is roughly the
    // frame rate (including display overhead, of course)
    if (frameID.load() > 0) m_fps.doneRender();

    // NOTE: consume a new renderer if one has been queued by another thread
    switchRenderers();

    if (m_resetAccum) {
      m_fb.clear(OSP_FB_ACCUM);
      m_resetAccum = false;
    }

    m_fps.startRender();
    //}

    ++frameID;

    if (viewPort.modified) {
      static bool once = true;
      if(once) {
        m_viewPort = viewPort;
        once = false;
      }
      Assert2(m_camera.handle(),"ospray camera is null");
      m_camera.set("pos", viewPort.from);
      auto dir = viewPort.at - viewPort.from;
      m_camera.set("dir", dir);
      m_camera.set("up", viewPort.up);
      m_camera.set("aspect", viewPort.aspect);
      m_camera.set("fovy", viewPort.openingAngle);
      m_camera.commit();
      viewPort.modified = false;
      m_accumID=0;
      m_fb.clear(OSP_FB_ACCUM);

      if (m_useDisplayWall)
        displayWall.fb.clear(OSP_FB_ACCUM);
    }

    m_renderer.renderFrame(m_fb, OSP_FB_COLOR | OSP_FB_ACCUM);
    if (m_useDisplayWall) {
      m_renderer.renderFrame(displayWall.fb, OSP_FB_COLOR | OSP_FB_ACCUM);
    }
    ++m_accumID;

    // set the glut3d widget's frame buffer to the opsray frame buffer,
    // then display
    ucharFB = (uint32_t *)m_fb.map(OSP_FB_COLOR);
    frameBufferMode = Glut3DWidget::FRAMEBUFFER_UCHAR;
    Glut3DWidget::display();

    m_fb.unmap(ucharFB);

    // that pointer is no longer valid, so set it to null
    ucharFB = nullptr;

    std::string title("OSPRay Scripted GLUT Viewer");

    if (m_alwaysRedraw) {
      title += " (" + std::to_string((long double)m_fps.getFPS()) + " fps)";
      setTitle(title);
      forceRedraw();
    } else {
      setTitle(title);
    }
  }

  void ScriptedOSPGlutViewer::keypress(char key, const vec2i &where)
  {
    switch (key) {
    case ':':
      if (!m_scriptHandler.running()) {
        m_scriptHandler.start();
      }
      break;
    default:
      OSPGlutViewer::keypress(key,where);
    }
  }

}// namepace ospray
