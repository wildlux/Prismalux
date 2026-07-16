/* ══════════════════════════════════════════════════════════════
   main_app_controller_blender.cpp — AppControllerPage: Blender
   ==================================================================
   Tab BLENDER (bpy via socket TCP) — builder + slot. Split da
   main_app_controller.cpp/main_app_controller_slots.cpp (TODO D-8).
   ══════════════════════════════════════════════════════════════ */
#include "main_app_controller.h"
#include "../prismalux_paths.h"
#include "../log_bus.h"
#include "../dpi_utils.h"
#include "../widgets/model_combo_box.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QDialog>
#include <QTextBrowser>
#include <QTextEdit>
#include <QPushButton>
#include <QTcpSocket>
#include <QAbstractSocket>
#include <QNetworkAccessManager>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <functional>

namespace P = PrismaluxPaths;

/* ──────────────────────────────────────────────────────────────
   System prompt arrays — identici a kSysPrompts[6..9] di StrumentiPage
   ────────────────────────────────────────────────────────────── */
const char* kBlenderSys[] = {
    /* 0 — Crea Primitiva */
    "Sei un esperto di Blender Python API (bpy). Genera SOLO codice Python per aggiungere una primitiva 3D. "
    "REGOLA 1: la prima riga DEVE essere 'import bpy'. "
    "REGOLA 2: usa SEMPRE bpy.ops.mesh.primitive_*_add() — VIETATO costruire mesh da vertici. "
    "Primitive: primitive_cube_add, primitive_uv_sphere_add, primitive_cylinder_add, "
    "primitive_plane_add, primitive_cone_add, primitive_torus_add, primitive_circle_add, "
    "primitive_ico_sphere_add, primitive_grid_add, primitive_monkey_add. "
    "Parametri tipici: size=2, location=(0,0,0), rotation=(0,0,0). "
    "Non aggiungere variabili 'result'. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    /* 1 — Cambia Materiale (base) */
    "Sei un esperto di Blender Python API. Genera SOLO codice Python eseguibile in Blender. "
    "Prima riga OBBLIGATORIA: 'import bpy'. "
    "Crea o modifica un materiale Principled BSDF sull'oggetto attivo (bpy.context.active_object). "
    "Imposta base_color (RGBA tuple), metallic (0-1), roughness (0-1) secondo la richiesta. "
    "Pattern: mat = bpy.data.materials.new('Mat'); mat.use_nodes = True; "
    "bsdf = mat.node_tree.nodes['Principled BSDF']; bsdf.inputs['Base Color'].default_value = (r,g,b,1). "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    /* 2 — Trasla */
    "Sei un esperto di Blender Python API. Genera SOLO codice Python eseguibile in Blender. "
    "Prima riga OBBLIGATORIA: 'import bpy'. "
    "Usa bpy.context.active_object.location = (x, y, z). "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    /* 3 — Ruota */
    "Sei un esperto di Blender Python API. Genera SOLO codice Python eseguibile in Blender. "
    "Prima riga OBBLIGATORIA: 'import bpy'. "
    "Usa bpy.context.active_object.rotation_euler = (rx, ry, rz) con angoli in radianti. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    /* 4 — Scala */
    "Sei un esperto di Blender Python API. Genera SOLO codice Python eseguibile in Blender. "
    "Prima riga OBBLIGATORIA: 'import bpy'. "
    "Usa bpy.context.active_object.scale = (sx, sy, sz). "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    /* 5 — Visibilita */
    "Sei un esperto di Blender Python API. Genera SOLO codice Python eseguibile in Blender. "
    "Prima riga OBBLIGATORIA: 'import bpy'. "
    "Usa obj.hide_viewport e obj.hide_render per visibilita viewport/render. "
    "Per tutti gli oggetti: [setattr(o,'hide_viewport',True) for o in bpy.data.objects if 'pattern' in o.name]. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    /* 6 — Render base */
    "Sei un esperto di Blender Python API. Genera SOLO codice Python eseguibile in Blender. "
    "Prima riga OBBLIGATORIA: 'import bpy'. "
    "Usa bpy.context.scene.render per le impostazioni, bpy.ops.render.render(write_still=True) per avviare. "
    "Imposta scene.render.filepath='/tmp/render_out' e scene.render.image_settings.file_format='PNG'. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    /* 7 — Modifica Mesh (Edit Mode + bmesh) */
    "Sei un esperto di Blender Python API e bmesh. Genera SOLO codice Python eseguibile in Blender. "
    "Prima riga OBBLIGATORIA: 'import bpy'. "
    "Importa: import bpy, bmesh, mathutils. "
    "Entra in edit mode: bpy.ops.object.mode_set(mode='EDIT'). "
    "Ottieni mesh: bm = bmesh.from_edit_mesh(bpy.context.active_object.data). "
    "Operazioni mesh: bmesh.ops.extrude_face_region, bmesh.ops.bevel, bmesh.ops.loop_cut, "
    "bmesh.ops.dissolve_edges, bmesh.ops.subdivide_edges, bmesh.ops.inset_faces. "
    "Per selezione: usa bm.verts/edges/faces; imposta .select = True/False. "
    "Aggiorna: bmesh.update_edit_mesh(bpy.context.active_object.data). "
    "Esci: bpy.ops.object.mode_set(mode='OBJECT'). "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    /* 8 — Modifier (Subdivision / Mirror / Boolean / Array / Solidify / Bevel / Decimate) */
    "Sei un esperto di Blender Python API e i Modifier. Genera SOLO codice Python eseguibile in Blender. "
    "Prima riga OBBLIGATORIA: 'import bpy'. "
    "Pattern: obj = bpy.context.active_object; mod = obj.modifiers.new(name='Nome', type='TIPO'). "
    "Tipi disponibili e parametri chiave: "
    "SUBSURF (mod.levels, mod.render_levels, mod.subdivision_type='CATMULL_CLARK'/'SIMPLE'); "
    "MIRROR (mod.use_axis=(True,False,False), mod.use_clip=True, mod.mirror_object=altro_obj); "
    "BOOLEAN (mod.operation='UNION'/'DIFFERENCE'/'INTERSECT', mod.object=altro_obj, mod.solver='EXACT'); "
    "ARRAY (mod.count=3, mod.relative_offset_displace=(1.2,0,0)); "
    "SOLIDIFY (mod.thickness=0.01, mod.offset=-1.0); "
    "BEVEL (mod.width=0.05, mod.segments=3, mod.limit_method='ANGLE'); "
    "DECIMATE (mod.ratio=0.5, mod.decimate_type='COLLAPSE'); "
    "ARMATURE (mod.object=armature_obj, mod.use_vertex_groups=True); "
    "SCREW (mod.axis='Z', mod.steps=32, mod.angle=6.283). "
    "Applica con: bpy.ops.object.modifier_apply(modifier='Nome'). "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    /* 9 — Materiale PBR avanzato (nodi Shader) */
    "Sei un esperto di Blender Python API e Shader Nodes (PBR). Genera SOLO codice Python eseguibile in Blender. "
    "Prima riga OBBLIGATORIA: 'import bpy'. "
    "Crea materiale con nodi: mat = bpy.data.materials.new('PBR_Mat'); mat.use_nodes = True; "
    "nodes = mat.node_tree.nodes; links = mat.node_tree.links; nodes.clear(). "
    "Nodi da creare con nodes.new('ShaderNode...'): "
    "ShaderNodeBsdfPrincipled — inputs: 'Base Color'(RGBA), 'Metallic'(0-1), 'Roughness'(0-1), "
    "'IOR'(1.45), 'Alpha'(1), 'Emission Color'(RGBA), 'Emission Strength'(float), "
    "'Subsurface Weight'(0-1), 'Coat Weight'(0-1), 'Sheen Weight'(0-1). "
    "ShaderNodeTexImage — carica texture: n.image = bpy.data.images.load('/path/tex.png'); "
    "  collega n.outputs['Color'] a Principled 'Base Color'. "
    "ShaderNodeTexImage (roughness) — collega a 'Roughness'; usa n.image.colorspace_settings.name='Non-Color'. "
    "ShaderNodeNormalMap + ShaderNodeTexImage (normal map) — collega NormalMap.outputs['Normal'] a 'Normal'. "
    "ShaderNodeOutputMaterial — inputs: 'Surface'. "
    "Aggiungi texture UV: ShaderNodeTexCoord + ShaderNodeMapping per scala/offset. "
    "Assegna a oggetto: obj = bpy.context.active_object; obj.data.materials.append(mat). "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    /* 10 — HDRI + Luci (illuminazione scena) */
    "Sei un esperto di Blender Python API. Genera SOLO codice Python eseguibile in Blender. "
    "Prima riga OBBLIGATORIA: 'import bpy'. "
    "HDRI World: scene = bpy.context.scene; world = scene.world; world.use_nodes = True; "
    "nt = world.node_tree; nt.nodes.clear(); "
    "env = nt.nodes.new('ShaderNodeTexEnvironment'); bg = nt.nodes.new('ShaderNodeBackground'); "
    "out = nt.nodes.new('ShaderNodeOutputWorld'); "
    "env.image = bpy.data.images.load('/path/to/hdri.hdr'); "
    "nt.links.new(env.outputs['Color'], bg.inputs['Color']); "
    "nt.links.new(bg.outputs['Background'], out.inputs['Surface']); "
    "bg.inputs['Strength'].default_value = 1.0. "
    "Luci: bpy.ops.object.light_add(type='POINT'/'SUN'/'AREA'/'SPOT', location=(x,y,z)). "
    "light = bpy.context.active_object.data; light.energy = 1000.0; light.color = (r,g,b). "
    "AREA: light.shape='RECTANGLE'; light.size=2.0; light.size_y=1.0. "
    "SPOT: light.spot_size=0.785; light.spot_blend=0.15. "
    "SUN: light.angle=0.00872 (0.5 gradi). "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    /* 11 — Impostazioni Render (Cycles / EEVEE) */
    "Sei un esperto di Blender Python API. Genera SOLO codice Python eseguibile in Blender. "
    "Prima riga OBBLIGATORIA: 'import bpy'. "
    "scene = bpy.context.scene. "
    "Motore: scene.render.engine = 'CYCLES' oppure 'BLENDER_EEVEE_NEXT'. "
    "Cycles: scene.cycles.samples = 256; scene.cycles.preview_samples = 32; "
    "scene.cycles.use_denoising = True; scene.cycles.denoiser = 'OPENIMAGEDENOISE'; "
    "scene.cycles.device = 'GPU'; scene.cycles.max_bounces = 8; "
    "scene.cycles.diffuse_bounces = 4; scene.cycles.glossy_bounces = 4; "
    "scene.cycles.transmission_bounces = 12. "
    "EEVEE: scene.eevee.taa_render_samples = 64; scene.eevee.use_bloom = True; "
    "scene.eevee.use_ssr = True; scene.eevee.use_ssr_refraction = True; "
    "scene.eevee.shadow_cube_size = '1024'. "
    "Output: scene.render.filepath = '/tmp/render_'; "
    "scene.render.resolution_x = 1920; scene.render.resolution_y = 1080; "
    "scene.render.resolution_percentage = 100; "
    "scene.render.image_settings.file_format = 'PNG'/'JPEG'/'OPEN_EXR'; "
    "scene.render.image_settings.color_mode = 'RGBA'. "
    "Avvia render: bpy.ops.render.render(write_still=True). "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    /* 12 — Import / Export 3D (OBJ / FBX / GLTF / STL) */
    "Sei un esperto di Blender Python API. Genera SOLO codice Python eseguibile in Blender. "
    "Prima riga OBBLIGATORIA: 'import bpy'. "
    "Import: "
    "  OBJ: bpy.ops.wm.obj_import(filepath='/path/file.obj'). "
    "  FBX: bpy.ops.import_scene.fbx(filepath='/path/file.fbx'). "
    "  GLTF/GLB: bpy.ops.import_scene.gltf(filepath='/path/file.glb'). "
    "  STL: bpy.ops.import_mesh.stl(filepath='/path/file.stl'). "
    "  SVG: bpy.ops.import_curve.svg(filepath='/path/file.svg'). "
    "Export (usa pathlib.Path.home()/'Desktop'/ come cartella default): "
    "  OBJ: bpy.ops.wm.obj_export(filepath='/tmp/out.obj', export_materials=True). "
    "  FBX: bpy.ops.export_scene.fbx(filepath='/tmp/out.fbx', use_selection=False). "
    "  GLTF: bpy.ops.export_scene.gltf(filepath='/tmp/out.glb', export_format='GLB', "
    "         export_materials='EXPORT'). "
    "  STL: bpy.ops.export_mesh.stl(filepath='/tmp/out.stl', ascii=False). "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    /* 13 — UV Unwrap */
    "Sei un esperto di Blender Python API. Genera SOLO codice Python eseguibile in Blender. "
    "Prima riga OBBLIGATORIA: 'import bpy'. "
    "Seleziona oggetto e vai in EDIT mode: bpy.ops.object.mode_set(mode='EDIT'). "
    "Seleziona tutto: bpy.ops.mesh.select_all(action='SELECT'). "
    "Metodi UV: "
    "  Unwrap standard (seams): bpy.ops.uv.unwrap(method='ANGLE_BASED', margin=0.001). "
    "  Smart UV (auto-seam): bpy.ops.uv.smart_project(angle_limit=66.0, "
    "    margin_method='FRACTION', island_margin=0.02). "
    "  Lightmap pack: bpy.ops.uv.lightmap_pack(PREF_MARGIN_DIV=0.1). "
    "Marca/rimuovi seam su spigoli selezionati: bpy.ops.mesh.mark_seam(clear=False). "
    "Normalizza UV island: bpy.ops.uv.average_islands_scale(). "
    "Comprimi UV in 0-1: bpy.ops.uv.pack_islands(margin=0.02). "
    "Torna in OBJECT: bpy.ops.object.mode_set(mode='OBJECT'). "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    /* 14 — Script libero */
    "Sei un esperto di Blender Python API (bpy). Genera SOLO codice Python eseguibile in Blender. "
    "REGOLA 1: la prima riga DEVE essere 'import bpy' (obbligatorio, altrimenti il codice fallisce). "
    "REGOLA 2: per forme 3D usa bpy.ops.mesh.primitive_*_add() — non costruire mesh da vertici. "
    "Namespace disponibile: bpy, bmesh, mathutils, pathlib, os, math. "
    "Non aggiungere variabili 'result'. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    nullptr
};

static const char* kBlenderActions[] = {
    "\xf0\x9f\xa7\x8a Crea Primitiva",
    "\xf0\x9f\x8e\xa8 Materiale base",
    "\xe2\x86\x94 Trasla",
    "\xf0\x9f\x94\x84 Ruota",
    "\xf0\x9f\x93\x90 Scala",
    "\xf0\x9f\x91\x81 Visibilit\xc3\xa0",
    "\xf0\x9f\x8e\xac Render base",
    "\xf0\x9f\xa7\xb1 Modifica Mesh",
    "\xf0\x9f\xaa\x84 Modifier",
    "\xf0\x9f\x8c\x9f Materiale PBR (nodi)",
    "\xf0\x9f\x92\xa1 HDRI + Luci",
    "\xf0\x9f\x96\xa5 Render avanzato",
    "\xf0\x9f\x93\xa6 Import / Export 3D",
    "\xf0\x9f\x97\xba UV Unwrap",
    "\xf0\x9f\x90\x8d Script libero",
    nullptr
};

/* ══════════════════════════════════════════════════════════════
   Tab BLENDER
   ══════════════════════════════════════════════════════════════ */
QWidget* AppControllerPage::buildBlenderTab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);

    auto* descLbl = new QLabel(
        "\xf0\x9f\x8e\xa8  <i>Blender \xe2\x80\x94 Software open-source di modellazione 3D, "
        "animazione, sculpting e rendering. Usato da artisti, designer e sviluppatori di videogiochi.</i>", w);
    descLbl->setObjectName("hintLabel");
    descLbl->setTextFormat(Qt::RichText);
    descLbl->setWordWrap(true);
    lay->addWidget(descLbl);

    /* ── Barra connessione ── */
    auto* connRow = new QWidget(w);
    auto* connLay = new QHBoxLayout(connRow);
    connLay->setContentsMargins(0, 0, 0, 0);
    connLay->setSpacing(8);

    auto* lbl = new QLabel(tr("Blender:"), connRow);
    lbl->setObjectName("hintLabel");

    m_blenderHostEdit = new QLineEdit("localhost:6789", connRow);
    m_blenderHostEdit->setFixedWidth(dpiScale(150));

    auto* pingBtn = new QPushButton(tr("\xf0\x9f\x94\x97  Verifica"), connRow);
    pingBtn->setToolTip(tr("Verifica che Blender sia in ascolto sulla porta WebSocket specificata"));
    pingBtn->setObjectName("actionBtn");
    pingBtn->setFixedWidth(dpiScale(100));

    m_blenderStatusLbl = new QLabel(tr("\xe2\x9a\xaa  Non connesso"), connRow);
    m_blenderStatusLbl->setObjectName("hintLabel");

    m_blenderExecBtn = new QPushButton(
        tr("\xe2\x96\xb6  Esegui in Blender"), connRow);
    m_blenderExecBtn->setObjectName("actionBtn");
    m_blenderExecBtn->setFixedWidth(dpiScale(160));
    m_blenderExecBtn->setEnabled(false);

    auto* helpBtn = new QPushButton(tr("\xf0\x9f\x9b\x9f  Aiuto"), connRow);
    helpBtn->setToolTip(tr("Apri la documentazione e i comandi AI disponibili per Blender"));
    helpBtn->setObjectName("actionBtn");
    helpBtn->setFixedWidth(dpiScale(80));

    connLay->addWidget(lbl);
    connLay->addWidget(m_blenderHostEdit);
    connLay->addWidget(pingBtn);
    connLay->addWidget(m_blenderStatusLbl, 1);
    connLay->addWidget(m_blenderExecBtn);
    connLay->addWidget(helpBtn);
    lay->addWidget(connRow);

    /* ── Hint ── */
    auto* hintLbl = new QLabel(
        "\xf0\x9f\x93\xa6 <b>MCP non connesso?</b> "
        "Installa <a href='https://github.com/ahujasid/blender-mcp'>"
        "blender-mcp</a> \xe2\x86\x92 Blender N \xe2\x86\x92 MCP \xe2\x86\x92 Start.", w);
    hintLbl->setObjectName("hintLabel");
    hintLbl->setOpenExternalLinks(true);
    hintLbl->setWordWrap(true);
    lay->addWidget(hintLbl);

    /* ── Avviso modello troppo piccolo per Blender (nascosto di default) ── */
    m_blenderWarnLbl = new QLabel(w);
    m_blenderWarnLbl->setObjectName("hintLabel");
    m_blenderWarnLbl->setWordWrap(true);
    m_blenderWarnLbl->setTextFormat(Qt::RichText);
    m_blenderWarnLbl->setStyleSheet(
        "background:#3a3000; border:1px solid #a08000;"
        " border-radius:4px; padding:6px; color:#f0d060;");
    m_blenderWarnLbl->setVisible(false);
    lay->addWidget(m_blenderWarnLbl);

    /* ── Azione + Modello ── */
    auto* toolRow = new QWidget(w);
    auto* toolLay = new QHBoxLayout(toolRow);
    toolLay->setContentsMargins(0, 0, 0, 0);
    toolLay->setSpacing(8);

    m_blenderAction = new QComboBox(toolRow);
    for (int i = 0; kBlenderActions[i]; i++)
        m_blenderAction->addItem(QString::fromUtf8(kBlenderActions[i]));
    /* Script libero come default: il modello capisce da solo cosa fare */
    m_blenderAction->setCurrentIndex(m_blenderAction->count() - 1);

    m_blenderModel = new ModelComboBox(m_ai, toolRow);

    toolLay->addWidget(new QLabel(tr("Azione:"), toolRow));
    toolLay->addWidget(m_blenderAction, 1);
    toolLay->addWidget(new QLabel(tr("Modello:"), toolRow));
    toolLay->addWidget(m_blenderModel, 1);
    lay->addWidget(toolRow);

    /* ── Input ── */
    m_blenderInput = new QTextEdit(w);
    m_blenderInput->setPlaceholderText(
        tr("Descrivi cosa fare in Blender — es.:\n"
        "  Crea una sfera rossa metallica con roughness 0.1\n"
        "  Aggiungi modifier Subdivision level 2 al cubo\n"
        "  Imposta render Cycles 256 sample, output PNG 1920x1080\n"
        "  Crea materiale PBR con texture diffuse e normal map\n"
        "  Aggiungi una HDRI e tre luci area\n"
        "  Esporta la scena in GLB"));
    m_blenderInput->setMaximumHeight(dpiScale(80));
    m_blenderInput->setMinimumHeight(dpiScale(60));
    lay->addWidget(m_blenderInput);

    /* ── Run/Stop ── */
    auto* btnRow = new QWidget(w);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    btnLay->setSpacing(8);
    m_blenderRunBtn  = new QPushButton(tr("\xe2\x96\xb6  Genera codice AI"), btnRow);
    m_blenderRunBtn->setObjectName("actionBtn");
    m_blenderStopBtn = new QPushButton(tr("\xe2\x8f\xb9  Stop"), btnRow);
    m_blenderStopBtn->setObjectName("actionBtn");
    m_blenderStopBtn->setProperty("danger", true);
    m_blenderStopBtn->setEnabled(false);
    btnLay->addWidget(m_blenderRunBtn);
    btnLay->addWidget(m_blenderStopBtn);
    btnLay->addStretch();
    lay->addWidget(btnRow);

    /* ── Output AI ── */
    m_blenderOutput = new QTextEdit(w);
    m_blenderOutput->setReadOnly(true);
    m_blenderOutput->setObjectName("outputView");
    m_blenderOutput->setPlaceholderText(tr("Output AI apparirà qui..."));
    m_blenderOutput->setMaximumHeight(dpiScale(160));
    lay->addWidget(m_blenderOutput, 0);

    /* ── Editor codice Python (diretto + popolato dall'AI) ── */
    auto* codeLbl = new QLabel(
        "\xf0\x9f\x90\x8d  <b>Codice Python da eseguire in Blender</b> "
        "<span style='color:#888;font-size:11px;'>"
        "(generato dall'AI o scrivi direttamente)</span>", w);
    codeLbl->setTextFormat(Qt::RichText);
    lay->addWidget(codeLbl);
    m_blenderCodeEdit = new QTextEdit(w);
    m_blenderCodeEdit->setObjectName("outputView");
    m_blenderCodeEdit->setPlaceholderText(
        tr("# Il codice Python generato dall'AI apparirà qui.\n"
        "# Puoi anche scrivere direttamente codice bpy:\n"
        "import bpy\n"
        "bpy.ops.mesh.primitive_cube_add(size=2, location=(0, 0, 0))"));
    m_blenderCodeEdit->setFont(QFont("Monospace", 10));
    m_blenderCodeEdit->setMinimumHeight(dpiScale(120));
    lay->addWidget(m_blenderCodeEdit, 1);

    /* ── Connessioni ── */
    m_blenderNam = new QNetworkAccessManager(this);

    connect(m_blenderCodeEdit, &QTextEdit::textChanged,
            this, &AppControllerPage::onBlenderCodeChanged);
    connect(pingBtn,           &QPushButton::clicked,
            this, &AppControllerPage::onBlenderPingClicked);
    connect(m_blenderExecBtn,  &QPushButton::clicked,
            this, &AppControllerPage::onBlenderExecClicked);
    connect(helpBtn,           &QPushButton::clicked,
            this, &AppControllerPage::onBlenderHelpClicked);
    connect(m_blenderRunBtn,   &QPushButton::clicked,
            this, &AppControllerPage::onBlenderRunClicked);
    connect(m_blenderStopBtn,  &QPushButton::clicked,
            this, &AppControllerPage::onBlenderStopClicked);

    return w;
}

/* ══════════════════════════════════════════════════════════════
   blenderSendTcp — helper TCP null-terminated per Blender MCP
   buf è QSharedPointer: safe anche se readyRead spara più volte
   o se timeout e readyRead coincidono — nessun double-free.
   ══════════════════════════════════════════════════════════════ */
void AppControllerPage::blenderSendTcp(const QString& host, int port,
                                       const QByteArray& jsonMsg,
                                       std::function<void(const QJsonObject&, bool)> cb)
{
    auto* sock = new QTcpSocket(this);
    auto  buf  = QSharedPointer<QByteArray>::create();
    /* done: flag one-shot per evitare di chiamare cb due volte */
    auto done  = QSharedPointer<bool>::create(false);
    sock->connectToHost(host, static_cast<quint16>(port));

    connect(sock, &QTcpSocket::connected, sock, [sock, jsonMsg]() {
        sock->write(jsonMsg + '\0');
        sock->flush();
    });
    connect(sock, &QTcpSocket::readyRead, sock, [sock, buf, cb, done]() {
        if (*done) return;
        buf->append(sock->readAll());
        const int nullPos = buf->indexOf('\0');
        if (nullPos < 0) return;          // risposta parziale — aspetta
        *done = true;
        const QByteArray resp = buf->left(nullPos);
        sock->disconnectFromHost();
        sock->deleteLater();
        const QJsonObject obj = QJsonDocument::fromJson(resp).object();
        cb(obj, obj.contains("result") || obj.value("ok").toBool(true));
    });
    connect(sock, &QAbstractSocket::errorOccurred, sock,
            [sock, cb, done](QAbstractSocket::SocketError) {
        if (*done) return;
        *done = true;
        const QString err = sock->errorString();
        sock->deleteLater();
        cb(QJsonObject{{"error", err}}, false);
    });
    QTimer::singleShot(8000, sock, [sock, cb, done]() {
        if (*done) return;
        *done = true;
        sock->abort();
        sock->deleteLater();
        cb(QJsonObject{{"error", "Timeout (8s)"}}, false);
    });
}

/* ======================================================================
   Sezione 3 — Blender tab slots
   ====================================================================== */

void AppControllerPage::onBlenderCodeChanged()
{
    m_blenderExecBtn->setEnabled(
        !m_blenderCodeEdit->toPlainText().trimmed().isEmpty());
}

void AppControllerPage::onBlenderPingClicked()
{
    QString addr = m_blenderHostEdit->text().trimmed();
    if (addr.isEmpty()) addr = "localhost:6789";
    const QString host = addr.contains(':') ? addr.section(':', 0, 0) : addr;
    const int     port = addr.contains(':') ? addr.section(':', 1).toInt() : 6789;
    m_blenderStatusLbl->setText(tr("\xf0\x9f\x94\x84  Connessione..."));
    QJsonObject req;
    req["type"]        = "execute";
    req["code"]        = "import bpy; result = {'ok': True, 'blender': bpy.app.version_string, "
                         "'objects': len(bpy.data.objects)}";
    req["strict_json"] = true;
    blenderSendTcp(host, port,
                   QJsonDocument(req).toJson(QJsonDocument::Compact),
                   [this](const QJsonObject& res, bool ok) {
        if (ok) {
            const QJsonObject r = res.value("result").toObject();
            const QString ver   = r.value("blender").toString(
                                      res.value("blender").toString("?"));
            const int     objs  = r.value("objects").toInt();
            m_blenderStatusLbl->setText(
                "\xe2\x9c\x85  Blender " + ver
                + "  \xc2\xb7  " + QString::number(objs) + " oggetti");
        } else {
            const QString blenderErr = res.value("error").toString("non raggiungibile");
            m_blenderStatusLbl->setText(tr("\xe2\x9d\x8c  ") + blenderErr);
            LogBus::post("\xe2\x9d\x8c Blender: Ping fallito: " + blenderErr);
        }
    });
}

void AppControllerPage::onBlenderExecClicked()
{
    const QString code = m_blenderCodeEdit->toPlainText().trimmed();
    if (code.isEmpty()) return;
    m_blenderCode = code;
    QString addr = m_blenderHostEdit->text().trimmed();
    if (addr.isEmpty()) addr = "localhost:6789";
    const QString host = addr.contains(':') ? addr.section(':', 0, 0) : addr;
    const int     port = addr.contains(':') ? addr.section(':', 1).toInt() : 6789;

    m_blenderExecBtn->setEnabled(false);
    m_blenderStatusLbl->setText(tr("\xf0\x9f\x94\x84  Verifica connessione..."));

    /* Ping automatico prima di eseguire */
    QJsonObject pingReq;
    pingReq["type"]        = "execute";
    pingReq["code"]        = "import bpy; result = {'ok': True, 'blender': bpy.app.version_string, "
                             "'objects': len(bpy.data.objects)}";
    pingReq["strict_json"] = true;
    blenderSendTcp(host, port,
                   QJsonDocument(pingReq).toJson(QJsonDocument::Compact),
                   [this, host, port, code](const QJsonObject& pingRes, bool pingOk) {
        if (!pingOk) {
            m_blenderExecBtn->setEnabled(true);
            const QString blenderPingErr = pingRes.value("error").toString("connessione rifiutata");
            m_blenderStatusLbl->setText(tr("\xe2\x9d\x8c  Non raggiungibile: ") + blenderPingErr);
            m_blenderOutput->append(
                "\n\xe2\x9d\x8c  Blender non connesso. "
                "Avvia il server MCP in Blender (N \xe2\x86\x92 MCP \xe2\x86\x92 Start).");
            LogBus::post("\xe2\x9d\x8c Blender: Non raggiungibile: " + blenderPingErr);
            return;
        }
        const QJsonObject r   = pingRes.value("result").toObject();
        const QString     ver = r.value("blender").toString("?");
        const int         objs = r.value("objects").toInt();
        m_blenderStatusLbl->setText(
            "\xf0\x9f\x94\x84  Blender " + ver + " \xc2\xb7 "
            + QString::number(objs) + " oggetti \xe2\x80\x94 Invio codice...");

        QJsonObject req;
        req["type"]        = "execute";
        req["code"]        = code;
        req["strict_json"] = true;
        blenderSendTcp(host, port,
                       QJsonDocument(req).toJson(QJsonDocument::Compact),
                       [this](const QJsonObject& res, bool ok) {
            m_blenderExecBtn->setEnabled(true);
            const QString raw = QString::fromUtf8(
                QJsonDocument(res).toJson(QJsonDocument::Compact));
            if (ok) {
                const QJsonValue rv = res.value("result");
                QString out;
                if (rv.isString())      out = rv.toString();
                else if (!rv.isNull())  out = QString::fromUtf8(
                    QJsonDocument(rv.toObject()).toJson(QJsonDocument::Compact));
                else                    out = raw;
                m_blenderStatusLbl->setText(tr("\xe2\x9c\x85  Eseguito in Blender"));
                m_blenderOutput->append("\n\xe2\x9c\x85  Blender: "
                    + (out.isEmpty() ? "OK" : out));
            } else {
                const QString blenderExecErr = res.value("error").toString(raw);
                m_blenderStatusLbl->setText(tr("\xe2\x9d\x8c  Errore esecuzione"));
                m_blenderOutput->append("\n\xe2\x9d\x8c  Blender: " + blenderExecErr);
                LogBus::post("\xe2\x9d\x8c Blender: Errore esecuzione: " + blenderExecErr);
            }
        });
    });
}

void AppControllerPage::onBlenderHelpClicked()
{
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(tr("\xf0\x9f\x8e\xa8  Installazione Blender MCP"));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(540, 480);
    auto* dlay    = new QVBoxLayout(dlg);
    auto* browser = new QTextBrowser(dlg);
    browser->setOpenExternalLinks(true);
    browser->setHtml(
        "<h3>\xf0\x9f\x8e\xa8 Blender MCP (addon ufficiale)</h3>"
        "<p style='background:#2a3a2a; border-left:4px solid #8c8; padding:8px; border-radius:4px;'>"
        "\xf0\x9f\x93\xa6 <b>File gi\xc3\xa0 inclusi in Prismalux</b><br>"
        "Nella cartella <code>MCPs/blender_addon/ADDONS_INSTALLAZIONE/</code> trovi:<br>"
        "&bull; <code>mcp-1.0.0.zip</code> \xe2\x80\x94 addon MCP ufficiale (installabile direttamente in Blender)<br>"
        "&bull; <code>blender_mcp_community.py</code> \xe2\x80\x94 versione community alternativa<br>"
        "&bull; <code>prismalux_bridge.py</code> \xe2\x80\x94 bridge Prismalux per esecuzione diretta</p>"
        "<h4>1. Installa Blender 5.1+</h4>"
        "<p><a href='https://www.blender.org/download/'>blender.org/download</a></p>"
        "<h4>2. Installa l'addon MCP</h4>"
        "<p>Blender \xe2\x86\x92 Edit \xe2\x86\x92 Preferences \xe2\x86\x92 Add-ons \xe2\x86\x92 <b>Install</b> "
        "\xe2\x86\x92 seleziona <code>mcp-1.0.0.zip</code> dalla cartella sopra \xe2\x86\x92 "
        "abilita \xe2\x86\x92 imposta porta <b>6789</b></p>"
        "<h4>3. Avvia il server</h4>"
        "<p>Il server parte automaticamente (Auto Start) oppure vai in "
        "3D Viewport \xe2\x86\x92 N \xe2\x86\x92 tab MCP \xe2\x86\x92 <b>Start MCP Server</b> (porta 6789).</p>"
        "<h4>4. Connetti</h4>"
        "<p>Torna qui \xe2\x86\x92 clicca <b>\xf0\x9f\x94\x97 Verifica</b>.<br>"
        "Protocollo: TCP socket JSON null-terminated (porta 6789).</p>"
        "<p><i>L'AI gira via Ollama e genera codice Python eseguito direttamente in Blender via TCP. "
        "llama.cpp non \xc3\xa8 richiesto.</i></p>");
    auto* btnClose = new QPushButton(tr("\xe2\x9c\x95  Chiudi"), dlg);
    btnClose->setObjectName("actionBtn");
    connect(btnClose, &QPushButton::clicked, dlg, &QDialog::accept);
    dlay->addWidget(browser);
    dlay->addWidget(btnClose);
    dlg->exec();
}

void AppControllerPage::onBlenderRunClicked()
{
    const int idx = m_blenderAction->currentIndex();
    if (idx < 0 || !kBlenderSys[idx]) return;

    /* ── Controllo modello troppo piccolo per Blender ── */
    if (m_blenderWarnLbl) {
        const QString modelName = m_blenderModel
            ? m_blenderModel->currentData().toString()
            : m_ai->model();
        QRegularExpression reBillion(R"((\d+(\.\d+)?)\s*[bB])");
        QRegularExpressionMatch m = reBillion.match(modelName);
        if (m.hasMatch()) {
            const double billions = m.captured(1).toDouble();
            if (billions < 7.0) {
                m_blenderWarnLbl->setText(
                    "\xe2\x9a\xa0\xef\xb8\x8f <b>Il modello <i>"
                    + modelName
                    + "</i> potrebbe essere troppo piccolo per Blender.</b> "
                    "Raccomandato: modello \xe2\x89\xa5 7B "
                    "(es. llama3.1:8b, qwen2.5-coder:7b).");
                m_blenderWarnLbl->setVisible(true);
            } else {
                m_blenderWarnLbl->setVisible(false);
            }
        } else {
            m_blenderWarnLbl->setVisible(false);
        }
    }

    runAi(0, QString::fromUtf8(kBlenderSys[idx]),
          m_blenderInput->toPlainText(),
          m_blenderOutput, m_blenderRunBtn, m_blenderStopBtn,
          m_blenderModel);
}

void AppControllerPage::onBlenderStopClicked()
{
    m_ai->abort();
    m_blenderRunBtn->setEnabled(true);
    m_blenderStopBtn->setEnabled(false);
    m_blenderOutput->append("\n\xe2\x8f\xb9  Fermato.");
}
