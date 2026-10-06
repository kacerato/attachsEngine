// Included inside editor_screen.cpp's UI namespace; no independent UI state.
void colliderToolButton(ScreenBuilder &b, UiRect rect, const char *text,
                        EditorWidget widget, UiIcon icon, bool active = false,
                        bool enabled = true) {
  const auto &t = b.theme;
  const auto hit = rect;
  if (active)
    b.list.addRect({rect.x, rect.bottom() - 3, rect.width, 3}, t.color.accent);
  if (rect.width > 78)
    b.list.addImage(centred(takeLeft(rect, 24), 16, 16),
                    static_cast<UiImageId>(icon),
                    enabled ? t.color.text : t.color.textMuted);
  b.label(rect, fitMiddle(b.list, text, rect.width, t.type.caption),
          enabled ? (active ? t.color.accent : t.color.text)
                  : t.color.textMuted,
          t.type.caption, UiAlign::Center);
  if (enabled)
    b.router.addRegion(hit, widgetId(widget));
}
void buildColliderAuthoringInspector(ScreenBuilder &b, UiRect content) {
  const auto &s = b.state;
  const auto &t = b.theme;
  const auto *d = s.colliderTopology;
  const bool roomy = content.height > 420;
  const bool details = s.colliderAuthoringDetails;
  auto header = takeTop(content, 38);
  b.iconButton(takeLeft(header, 36), UiIcon::UiArrowLeft,
               widgetId(d ? EditorWidget::ColliderGeometryClose
                          : EditorWidget::PhysicsDiagnosticClose));
  b.iconButton(takeRight(header, 36), UiIcon::EditorAuthorMore,
               widgetId(EditorWidget::ColliderAuthoringDetails), details);
  b.label(header, d ? "Geometria de colisão" : "Diagnóstico físico",
          t.color.text, t.type.body);
  const auto line = [&](const std::string &text, UiColor color) {
    if (content.height >= 20) {
      auto row = takeTop(content, 23);
      b.label(row, fitMiddle(b.list, text, row.width, t.type.caption), color,
              t.type.caption);
    }
  };
  if (d) {
    auto footer = takeBottom(content, 42);
    colliderToolButton(b, takeLeft(footer, footer.width * .5f), "Descartar",
                       EditorWidget::ColliderGeometryClose,
                       UiIcon::RuntimeStop);
    colliderToolButton(b, footer, d->converted ? "Converter" : "Aplicar",
                       EditorWidget::ColliderGeometryApply, UiIcon::AssetsSave,
                       true, d->valid);
    auto modes = takeTop(content, 40);
    const float half = modes.width * .5f;
    colliderToolButton(b, takeLeft(modes, half), "Faces",
                       EditorWidget::ColliderGeometryFace,
                       UiIcon::EditorColliderFace, !d->vertexMode);
    colliderToolButton(b, modes, "Vértices",
                       EditorWidget::ColliderGeometryVertex,
                       UiIcon::EditorColliderVertex, d->vertexMode);
    if (details || roomy) {
      auto choices = takeTop(content, 38);
      colliderToolButton(b, takeLeft(choices, half),
                         d->hidden ? "Ocultos: sim" : "Ocultos: não",
                         EditorWidget::ColliderGeometryHidden,
                         UiIcon::EditorAuthorEye, d->hidden);
      colliderToolButton(b, choices, d->additive ? "Somar: sim" : "Somar: não",
                         EditorWidget::ColliderGeometryAdditive,
                         UiIcon::EditorAuthorObject, d->additive);
      line(std::to_string(d->vertices.size()) + " vértices · " +
               std::to_string(d->faces.size()) + " faces · " +
               std::to_string(d->indices.size() / 3) + " triângulos",
           t.color.textDim);
      line(d->converted ? "Aplicar converte esta primitiva em Malha"
           : d->convex  ? "Casco Jolt · pontos interiores não colidem"
                        : "Malha côncava · topologia física independente",
           d->converted ? t.color.warning : t.color.textDim);
      if (d->converted && d->vertices.size() > 8)
        line("Superfície curva aproximada por polígonos", t.color.warning);
    }
    if (!details) {
      ColliderTopology::Point center;
      const bool selection = d->centroid(center);
      line(selection ? std::to_string(d->selected.size()) +
                           (d->vertexMode ? " vértice(s) selecionado(s)"
                                          : " face(s) selecionada(s)")
                     : "Toque no viewport para selecionar",
           selection ? t.color.accent : t.color.textDim);
      if (selection) {
        if (roomy)
          line("Centro da seleção · coordenadas locais", t.color.textDim);
        auto numbers = takeTop(content, 40);
        const float width = numbers.width / 3;
        const UiColor colors[]{t.color.axisX, t.color.axisY, t.color.axisZ};
        for (u32 axis = 0; axis < 3; ++axis) {
          auto cell = takeLeft(numbers, width);
          const auto hit = cell;
          char text[40];
          std::snprintf(text, sizeof text, "%c  %.4g", "XYZ"[axis],
                        double(center[axis]));
          b.list.addRect(deflate(cell, UiInsets::all(2)), t.color.silhouette,
                         2);
          b.label(cell, text, colors[axis], t.type.caption, UiAlign::Center);
          b.router.addRegion(
              hit, widgetId(EditorWidget::ColliderGeometryCoordinateX) + axis);
        }
      }
      if (roomy) {
        auto history = takeTop(content, 36);
        b.iconButton(takeLeft(history, 36), UiIcon::EditorUndo,
                     widgetId(EditorWidget::ColliderGeometryUndo), false,
                     t.color.textDim, !d->undo.empty());
        b.iconButton(takeLeft(history, 36), UiIcon::EditorRedo,
                     widgetId(EditorWidget::ColliderGeometryRedo), false,
                     t.color.textDim, !d->redo.empty());
        b.label(history, "Histórico da prévia", t.color.textDim,
                t.type.caption);
      }
      line(d->valid ? "Prévia validada no Jolt · visual preservado" : d->error,
           d->valid ? t.color.accent : t.color.warning);
      if (!roomy && d->converted)
        line(d->vertices.size() > 8
                 ? "Converter: curva aproximada em polígonos"
                 : "Converter: primitiva → Malha independente",
             t.color.warning);
    }
    if (details || roomy) {
      char cost[140];
      std::snprintf(cost, sizeof cost,
                    "Extrair %.2f · validar %.2f · pick %.2f ms", d->buildMs,
                    d->validateMs, d->pickMs);
      line(cost, t.color.textMuted);
      std::snprintf(cost, sizeof cost, "UI + seleção: %.2f ms por atualização",
                    s.colliderUiMs);
      line(cost, t.color.textMuted);
      line("Desenho: até 800 triângulos / 512 pontos", t.color.textMuted);
      line("Picking e publicação usam toda a geometria", t.color.textMuted);
    }
  } else {
    auto actions = takeTop(content, 38);
    b.iconButton(takeRight(actions, 36), UiIcon::AssetsImport,
                 widgetId(EditorWidget::PhysicsDiagnosticRefresh));
    b.label(actions,
            s.physicsDiagnosticLive ? "Execução · leitura do solver"
                                    : "Autoria · sem scripts ou passos físicos",
            t.color.textDim, t.type.caption);
    const struct {
      const char *label;
      EditorWidget widget;
      UiIcon icon;
      bool active;
    } filters[]{
        {"Visual · contorno azul", EditorWidget::PhysicsDiagnosticVisual,
         UiIcon::AssetsStaticMesh, s.physicsDiagnosticVisual},
        {"Colisão · contorno branco", EditorWidget::PhysicsDiagnosticCollision,
         UiIcon::ComponentCollider, s.physicsDiagnosticCollision},
        {"Apoio · pontos e normal", EditorWidget::PhysicsDiagnosticSupport,
         UiIcon::PhysicsDiagnostic, s.physicsDiagnosticSupport}};
    if (details || roomy)
      for (const auto &f : filters)
        colliderToolButton(b, takeTop(content, 36), f.label, f.widget, f.icon,
                           f.active);
    if (details || roomy)
      line("COM · marca amarela · leitura, não centro local", t.color.warning);
    if (!s.physicsDiagnosticError.empty())
      line(s.physicsDiagnosticError, t.color.warning);
    else {
      std::istringstream input(s.physicsDiagnosticText);
      std::string text;
      while (std::getline(input, text)) {
        const bool extra =
            text.starts_with("Caixas") || text.starts_with("Malhas") ||
            text.starts_with("Velocidade") || text.starts_with("Ativo") ||
            text.starts_with("Dormindo") || text.starts_with("Estático");
        if (details ? extra : !extra)
          line(text, t.color.text);
      }
    }
    if (details) {
      char cost[96];
      std::snprintf(cost, sizeof cost,
                    "Consulta %.2f ms · %llu montagens autorais",
                    s.physicsDiagnosticMs,
                    static_cast<unsigned long long>(s.physicsDiagnosticBuilds));
      line(cost, t.color.textMuted);
      std::snprintf(cost,sizeof cost,"UI + diagnóstico %.2f ms",s.colliderUiMs);
      line(cost,t.color.textMuted);
      line("Prévia reutilizada até a cena mudar", t.color.textMuted);
      line("Fechar desativa consultas e libera a prévia", t.color.textMuted);
    }
  }
}
void buildColliderTopologyOverlay(ScreenBuilder &b, const UiRect &viewport) {
  const auto *d = b.state.colliderTopology;
  if (!d || !d->active() || !b.state.view || b.state.cameraViewEntity)
    return;
  const auto &view = *b.state.view;
  const auto &t = b.theme;
  float pose[16];
  if (!colliderTopologyPose(*b.state.document, d->object, d->instance, pose))
    return;
  const auto visible = [&](const ColliderTopology::Point &p) {
    const auto projected = projectWorldToScreen(view, p.data());
    if (!projected.valid)
      return false;
    const auto ray = screenPointToRay(view, projected.screen);
    float depth = 0;
    for (u32 k = 0; k < 3; ++k)
      depth += (p[k] - ray.origin[k]) * ray.direction[k];
    return d->hidden ||
           !colliderPointOccluded(b.state.colliderHandleOccluders, view,
                                  projected.screen, d->object, depth);
  };
  const auto count = d->indices.size() / 3,
             stride = std::max<usize>(
                 1, (count + ColliderTopology::MaximumDrawTriangles - 1) /
                        ColliderTopology::MaximumDrawTriangles);
  for (usize i = 0; i < count; i += stride) {
    ColliderTopology::Point points[3], middle{};
    for (u32 v = 0; v < 3; ++v) {
      points[v] = topologyWorldPoint(pose, d->vertices[d->indices[i * 3 + v]]);
      for (u32 k = 0; k < 3; ++k)
        middle[k] += points[v][k] / 3;
    }
    if (!visible(middle))
      continue;
    const bool selected =
        !d->vertexMode && std::find(d->selected.begin(), d->selected.end(),
                                    d->faceOfTriangle[i]) != d->selected.end();
    for (u32 e = 0; e < 3; ++e) {
      UiPoint a, z;
      if (projectSegmentToScreen(view, points[e].data(),
                                 points[(e + 1) % 3].data(), a, z))
        b.list.addLine(
            a, z, selected ? t.color.accent : withAlpha(t.color.text, .65f),
            selected ? 2.5f : 1.f);
    }
    if (selected) {
      auto a = projectWorldToScreen(view, points[0].data()),
           z = projectWorldToScreen(view, points[1].data()),
           c = projectWorldToScreen(view, points[2].data());
      if (a.valid && z.valid && c.valid)
        b.list.addTriangle({a.screen, {}, withAlpha(t.color.accent, .18f)},
                           {z.screen, {}, withAlpha(t.color.accent, .18f)},
                           {c.screen, {}, withAlpha(t.color.accent, .18f)});
    }
  }
  if (d->convex && d->dirty && d->valid) {
    const auto step = std::max<usize>(1, (d->cooked.size() + 799) / 800);
    for (usize i = 0; i < d->cooked.size(); i += step)
      for (u32 e = 0; e < 3; ++e) {
        ColliderTopology::Point p, q;
        std::copy_n(d->cooked[i].data() + e * 3, 3, p.data());
        std::copy_n(d->cooked[i].data() + (e + 1) % 3 * 3, 3, q.data());
        p = topologyWorldPoint(pose, p);
        q = topologyWorldPoint(pose, q);
        UiPoint a, z;
        if (visible(p) &&
            projectSegmentToScreen(view, p.data(), q.data(), a, z))
          b.list.addLine(a, z, t.color.axisZ, 1.2f);
      }
  }
  if (d->vertexMode) {
    const auto step = std::max<usize>(1, (d->vertices.size() + 511) / 512);
    const auto draw = [&](u32 i, bool selected) {
      const auto p = topologyWorldPoint(pose, d->vertices[i]);
      const auto projected = projectWorldToScreen(view, p.data());
      if (!projected.valid || !viewport.contains(projected.screen) ||
          !visible(p))
        return;
      if (!d->hidden) {
        const auto ray = screenPointToRay(view, projected.screen);
        float depth = 0, hit;
        for (u32 k = 0; k < 3; ++k)
          depth += (p[k] - ray.origin[k]) * ray.direction[k];
        if (d->surface.intersect(ray.origin, ray.direction, pose, hit,
                                 ray.minimumDistance,
                                 std::max(ray.minimumDistance, depth - .005f)))
          return;
      }
      const float size = selected ? 10.f : 6.f;
      b.list.addRect({projected.screen.x - size / 2,
                      projected.screen.y - size / 2, size, size},
                     selected ? t.color.accent : t.color.text,
                     selected ? 2 : 0);
    };
    for (u32 i = 0; i < d->vertices.size(); i += step)
      draw(i, false);
    const auto selectedStep =
        std::max<usize>(1, (d->selected.size() + 511) / 512);
    for (usize n = 0; n < d->selected.size(); n += selectedStep)
      if (d->selected[n] < d->vertices.size())
        draw(d->selected[n], true);
  }
  ColliderTopology::Point center;
  if (!d->centroid(center))
    return;
  const auto origin = topologyWorldPoint(pose, center);
  const auto projected = projectWorldToScreen(view, origin.data());
  if (!projected.valid)
    return;
  const float worldLength =
      72.f * 2 * renderer::projectionHalfHeight(view.frustum) *
      renderer::projectionDivisor(view.frustum, projected.viewDepth) /
      view.rect.height;
  const UiColor colors[]{t.color.axisX, t.color.axisY, t.color.axisZ};
  std::vector<UiPoint> grips;
  for (u32 axis = 0; axis < 3; ++axis) {
    float length = 0;
    for (u32 k = 0; k < 3; ++k)
      length += pose[axis * 4 + k] * pose[axis * 4 + k];
    length = std::sqrt(length);
    if (length < 1e-6f)
      continue;
    EditorCameraHandle handle;
    ColliderTopology::Point end;
    for (u32 k = 0; k < 3; ++k) {
      handle.point[k] = origin[k];
      handle.axis[k] = pose[axis * 4 + k] / length;
      end[k] = origin[k] + handle.axis[k] * worldLength;
    }
    auto p = projectWorldToScreen(view, end.data());
    float parameter;
    if (!p.valid ||
        !cameraHandleRayParameter(view, handle, p.screen, parameter) ||
        !visible(origin))
      continue;
    float dx = p.screen.x - projected.screen.x,
          dy = p.screen.y - projected.screen.y,
          screenLength = std::hypot(dx, dy);
    if (screenLength < 12)
      continue;
    dx /= screenLength;
    dy /= screenLength;
    UiPoint grip{projected.screen.x + dx * std::max(48.f, screenLength),
                 projected.screen.y + dy * std::max(48.f, screenLength)};
    for (u32 attempt = 0; attempt < 6; ++attempt) {
      bool overlap = false;
      for (auto old : grips)
        if (std::abs(old.x - grip.x) < 34 && std::abs(old.y - grip.y) < 34)
          overlap = true;
      if (!overlap)
        break;
      grip.x += dx * 34;
      grip.y += dy * 34;
    }
    const UiRect hit{grip.x - 16, grip.y - 16, 32, 32};
    if (!viewport.contains(grip))
      continue;
    grips.push_back(grip);
    b.list.addLine(projected.screen, grip, colors[axis], 2);
    b.list.addRect({grip.x - 6, grip.y - 6, 12, 12}, colors[axis], 1);
    b.router.addRegion(hit,
                       widgetId(EditorWidget::ColliderGeometryAxisX) + axis);
  }
}
void buildPhysicsDiagnosticOverlay(ScreenBuilder &b, bool drawCollision = false) {
  const auto &s = b.state;
  if (!s.physicsDiagnosticOpen || !s.view)
    return;
  const auto &view = *s.view;
  const auto &t = b.theme;
  if(s.physicsDiagnosticCollision && s.physicsDiagnosticHasCapsule) {
    // Read the active Jolt shape, never reconstruct it from authoring defaults.
    const float *bottom=s.physicsDiagnosticCapsuleBottom,*top=s.physicsDiagnosticCapsuleTop;
    const float radius=s.physicsDiagnosticCapsuleRadius;
    float up[3],right[3],forward[3];float length=0;
    for(u32 k=0;k<3;++k){up[k]=top[k]-bottom[k];length+=up[k]*up[k];}
    length=std::sqrt(length);
    if(length>1e-6f && radius>0) {
      for(float &v:up)v/=length;
      const float reference[3]{std::abs(up[0])<.9f?1.f:0.f,std::abs(up[0])<.9f?0.f:1.f,0};
      for(u32 k=0;k<3;++k)right[k]=up[(k+1)%3]*reference[(k+2)%3]-up[(k+2)%3]*reference[(k+1)%3];
      const float norm=std::sqrt(right[0]*right[0]+right[1]*right[1]+right[2]*right[2]);
      for(float &v:right)v/=norm;
      for(u32 k=0;k<3;++k)forward[k]=up[(k+1)%3]*right[(k+2)%3]-up[(k+2)%3]*right[(k+1)%3];
      const auto segment=[&](const float *center,const float *axis,float a,float z,bool cap,float sign) {
        float p[3],q[3];
        for(u32 k=0;k<3;++k) {
          p[k]=center[k]+radius*(axis[k]*std::cos(a)+(cap?up[k]*sign:forward[k])*std::sin(a));
          q[k]=center[k]+radius*(axis[k]*std::cos(z)+(cap?up[k]*sign:forward[k])*std::sin(z));
        }
        UiPoint x,y;if(projectSegmentToScreen(view,p,q,x,y))b.list.addLine(x,y,t.color.text,1.5f);
      };
      for(const float *center:{bottom,top})for(u32 n=0;n<24;++n)
        segment(center,right,n*6.28318530718f/24,(n+1)*6.28318530718f/24,false,0);
      for(const float *axis:{right,forward}) {
        for(u32 n=0;n<24;++n) {
          segment(bottom,axis,n*3.14159265359f/24,(n+1)*3.14159265359f/24,true,-1);
          segment(top,axis,n*3.14159265359f/24,(n+1)*3.14159265359f/24,true,1);
        }
        for(float side:{-1.f,1.f}) {
          float p[3],q[3];for(u32 k=0;k<3;++k){p[k]=bottom[k]+side*radius*axis[k];q[k]=top[k]+side*radius*axis[k];}
          UiPoint x,y;if(projectSegmentToScreen(view,p,q,x,y))b.list.addLine(x,y,t.color.text,1.5f);
        }
      }
    }
  }
  // Play uses a read-only overlay: no authoring gizmos or input regions.
  if (drawCollision && s.physicsDiagnosticCollision && s.document) {
    const float aspect=view.frustum.tangentHalfHorizontal/view.frustum.tangentHalfVertical;
    const auto visuals=collectComponentVisuals(*s.document,s.selection,aspect,s.resources,
        s.pathInstance,s.pathPointId,s.pathEntity,s.expandedNative);
    for(const auto &visual:visuals)if(visual.icon==UiIcon::ComponentCollider &&
        componentVisualVisible(*s.document,visual.entity,s.hiddenLayers,s.sceneHidden))
      for(const auto &line:visual.segments) {
        UiPoint a,z;
        if(projectSegmentToScreen(view,line.a,line.b,a,z))b.list.addLine(a,z,
            visual.entity==s.selection && visual.instance==s.expandedNative?t.color.text:withAlpha(t.color.textMuted,.5f),1.5f);
      }
  }
  if (s.physicsDiagnosticVisual && s.resources)
    if (const auto *entity = s.document->find(s.selection))
      if (const auto *mesh = meshRenderer(*entity)) {
        u32 drawn = 0;
        float world[16];
        if (editorWorldMatrix(*s.document, entity->id, world))
          for (u32 n = 0; n < mesh->slotCount() && drawn < 800; ++n) {
            std::span<const EditorPickMesh::Triangle> triangles;
            float relative[16], pose[16];
            if (!s.resources->localGeometry(mesh->slotMesh(n), triangles,
                                            relative))
              continue;
            visual_detail::multiply(world, relative, pose);
            const auto remaining = 800 - drawn;
            const auto step = std::max<usize>(
                1, (triangles.size() + remaining - 1) / remaining);
            for (usize i = 0; i < triangles.size() && drawn < 800;
                 i += step, ++drawn)
              for (u32 e = 0; e < 3; ++e) {
                ColliderTopology::Point p, q;
                std::copy_n(triangles[i].data() + e * 3, 3, p.data());
                std::copy_n(triangles[i].data() + (e + 1) % 3 * 3, 3, q.data());
                p = topologyWorldPoint(pose, p);
                q = topologyWorldPoint(pose, q);
                UiPoint a, z;
                if (projectSegmentToScreen(view, p.data(), q.data(), a, z))
                  b.list.addLine(a, z, t.color.axisZ, 1);
              }
          }
      }
  if (s.physicsDiagnosticHasCom) {
    const auto p = projectWorldToScreen(view, s.physicsDiagnosticCom);
    if (p.valid) {
      b.list.addImage({p.screen.x - 12, p.screen.y - 12, 24, 24},
                      static_cast<UiImageId>(UiIcon::PhysicsDiagnostic),
                      t.color.warning);
    }
  }
  if (s.physicsDiagnosticSupport) {
    for (const auto &point : s.physicsDiagnosticProbes) {
      const auto p = projectWorldToScreen(view, point.data());
      if (p.valid)
        b.list.addRect({p.screen.x - 3, p.screen.y - 3, 6, 6}, t.color.axisY,
                       1);
    }
    if (s.physicsDiagnosticHasSupport) {
      float end[3];
      for (u32 k = 0; k < 3; ++k)
        end[k] =
            s.physicsDiagnosticPoint[k] + s.physicsDiagnosticNormal[k] * .4f;
      UiPoint a, z;
      if (projectSegmentToScreen(view, s.physicsDiagnosticPoint, end, a, z)) {
        b.list.addLine(a, z, t.color.axisY, 3);
        b.list.addRect({a.x - 5, a.y - 5, 10, 10}, t.color.axisY, 5);
      }
    }
  }
}
