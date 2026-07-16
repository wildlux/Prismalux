/* ══════════════════════════════════════════════════════════════
   main_app_controller_freecad.cpp — AppControllerPage: FreeCAD
   ==================================================================
   Tab FREECAD (Python API via socket TCP) — builder + slot. Split da
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
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

namespace P = PrismaluxPaths;

const char* kFreeCADSys[] = {
    /* 0 — Crea Primitiva */
    "Sei un esperto di FreeCAD Python API. Genera SOLO codice Python puro eseguibile in FreeCAD. "
    "Usa: import FreeCAD, Part; doc = FreeCAD.activeDocument() or FreeCAD.newDocument('Doc'). "
    "Primitive disponibili: Part.makeBox(l,w,h), Part.makeCylinder(r,h), Part.makeSphere(r), "
    "Part.makeCone(r1,r2,h), Part.makeTorus(r1,r2), Part.makeWedge(...). "
    "Aggiungi con: obj = doc.addObject('Part::Feature','Nome'); obj.Shape = primitiva; doc.recompute(). "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    /* 1 — Crea Schizzo (Sketcher) */
    "Sei un esperto di FreeCAD Python API e Sketcher. Genera SOLO codice Python puro. "
    "Usa: import FreeCAD, Sketcher; doc = FreeCAD.activeDocument() or FreeCAD.newDocument('Doc'). "
    "sketch = doc.addObject('Sketcher::SketchObject','Sketch'); sketch.Placement = FreeCAD.Placement(). "
    "Aggiungi geometria: sketch.addGeometry(Part.LineSegment(v1,v2), False), "
    "Part.Circle(centro, asse, raggio), Part.ArcOfCircle(...). "
    "Vincoli: sketch.addConstraint(Sketcher.Constraint('Coincident',0,2,1,1)). "
    "sketch.addConstraint(Sketcher.Constraint('Horizontal',0)). "
    "sketch.addConstraint(Sketcher.Constraint('Distance',0,10.0)). "
    "doc.recompute(). Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    /* 2 — Booleana */
    "Sei un esperto di FreeCAD Python API. Genera SOLO codice Python puro per operazioni booleane. "
    "Usa Part.fuse(shape1,shape2) per unione, Part.cut(shape1,shape2) per differenza, "
    "Part.common(shape1,shape2) per intersezione. "
    "Accedi alle shape: s1 = doc.getObject('Nome1').Shape. "
    "Salva risultato: res = doc.addObject('Part::Feature','Risultato'); res.Shape = Part.fuse(s1,s2). "
    "doc.recompute(). Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    /* 3 — Esporta STL / STEP / IGES / OBJ */
    "Sei un esperto di FreeCAD Python API. Genera SOLO codice Python puro per esportare la geometria. "
    "STL: import Mesh; Mesh.export([doc.getObject('Nome')], '/tmp/output.stl'). "
    "STEP: import Import; Import.export([doc.getObject('Nome')], '/tmp/output.step'). "
    "IGES: Import.export([doc.getObject('Nome')], '/tmp/output.iges'). "
    "OBJ: import importOBJ; importOBJ.export([doc.getObject('Nome')], '/tmp/output.obj'). "
    "DXF: import importDXF; importDXF.export([doc.getObject('Nome')], '/tmp/output.dxf'). "
    "Usa pathlib.Path.home()/'Desktop'/ come cartella di default. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    /* 4 — Modifica proprieta */
    "Sei un esperto di FreeCAD Python API. Genera SOLO codice Python puro per modificare proprieta' di oggetti. "
    "Accedi: obj = FreeCAD.activeDocument().getObject('Nome'). "
    "Proprieta' tipiche: obj.Length, obj.Width, obj.Height (Box); obj.Radius, obj.Height (Cyl/Sphere). "
    "PartDesign Pad/Pocket: obj.Length (profondita'); obj.Midplane = True/False; obj.Reversed = True/False. "
    "Placement: obj.Placement = FreeCAD.Placement(FreeCAD.Vector(x,y,z), FreeCAD.Rotation(yaw,pitch,roll)). "
    "doc.recompute(). Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    /* 5 — Vincoli & misure */
    "Sei un esperto di FreeCAD Python API. Genera SOLO codice Python puro per vincoli e misure. "
    "Sketcher: sketch.addConstraint(Sketcher.Constraint('Coincident',geo1,pt1,geo2,pt2)). "
    "Tipi Sketcher.Constraint: 'Horizontal','Vertical','Parallel','Perpendicular','Equal', "
    "'Distance','DistanceX','DistanceY','Radius','Angle','Symmetric','Block'. "
    "Misure 3D Draft: import Draft; d = Draft.make_dimension(v1,v2,v_passante). "
    "Misura angolare: Draft.make_angular_dimension(centro, angoli, vettore). "
    "doc.recompute(). Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    /* 6 — Part Design (Pad / Pocket / Fillet / Chamfer / Pattern) */
    "Sei un esperto di FreeCAD Python API e PartDesign. Genera SOLO codice Python puro eseguibile in FreeCAD. "
    "import FreeCAD, Part, PartDesign, Sketcher; doc = FreeCAD.activeDocument() or FreeCAD.newDocument('Doc'). "
    "Body: body = doc.addObject('PartDesign::Body','Body'). "
    "Pad (estrusione): pad = body.newObject('PartDesign::Pad','Pad'); "
    "pad.Profile = sketch; pad.Length = 10.0; pad.Midplane = False; pad.Reversed = False. "
    "Pocket (tasca): pocket = body.newObject('PartDesign::Pocket','Pocket'); "
    "pocket.Profile = sketch; pocket.Length = 5.0; pocket.Type = 0. "
    "Fillet (raccordo): fillet = body.newObject('PartDesign::Fillet','Fillet'); "
    "fillet.Base = (solido, ['Edge1','Edge2']); fillet.Radius = 2.0. "
    "Chamfer (smusso): chamfer = body.newObject('PartDesign::Chamfer','Chamfer'); "
    "chamfer.Base = (solido, ['Edge1']); chamfer.Size = 1.0. "
    "Hole: hole = body.newObject('PartDesign::Hole','Hole'); hole.Profile = sketch; "
    "hole.Diameter = 6.0; hole.Depth = 15.0; hole.ThreadType = 1; hole.ModelThread = True. "
    "Linear Pattern: lp = body.newObject('PartDesign::LinearPattern','LP'); "
    "lp.Originals = [pad]; lp.Direction = (body, ['H_Axis']); lp.Length = 50.0; lp.Occurrences = 3. "
    "Polar Pattern: pp = body.newObject('PartDesign::PolarPattern','PP'); "
    "pp.Originals = [pad]; pp.Axis = (body, ['V_Axis']); pp.Angle = 360.0; pp.Occurrences = 6. "
    "doc.recompute(). Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    /* 7 — FEM Setup (mesh + vincoli + forze) */
    "Sei un esperto di FreeCAD Python API e FEM workbench. Genera SOLO codice Python puro. "
    "import FreeCAD, ObjectsFem; doc = FreeCAD.activeDocument() or FreeCAD.newDocument('FEM'). "
    "Analysis: analysis = ObjectsFem.makeAnalysis(doc, 'Analysis'). "
    "Mesh Gmsh: mesh = ObjectsFem.makeMeshGmsh(doc, 'FEMMeshGmsh'); "
    "mesh.Part = doc.getObject('NomeSolido'); mesh.CharacteristicLengthMax = 5.0; "
    "mesh.CharacteristicLengthMin = 1.0; mesh.ElementOrder = '2nd'. "
    "Genera mesh Gmsh: from femmesh.gmshtools import GmshTools; gt = GmshTools(mesh, doc); gt.create_mesh(). "
    "Fixed constraint: fix = ObjectsFem.makeConstraintFixed(doc, 'ConstraintFixed'); "
    "fix.References = [(doc.getObject('Solid'), 'Face1')]; analysis.addObject(fix). "
    "Force constraint: force = ObjectsFem.makeConstraintForce(doc, 'ConstraintForce'); "
    "force.References = [(doc.getObject('Solid'), 'Face2')]; "
    "force.Force = 1000.0; force.Reversed = False; analysis.addObject(force). "
    "Pressure: press = ObjectsFem.makeConstraintPressure(doc, 'ConstraintPressure'); "
    "press.References = [(doc.getObject('Solid'), 'Face3')]; press.Pressure = 10.0. "
    "doc.recompute(). Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    /* 8 — FEM Esegui analisi (CalculiX) */
    "Sei un esperto di FreeCAD Python API e FEM CalculiX. Genera SOLO codice Python puro. "
    "import FreeCAD, ObjectsFem; from femtools import ccxtools; doc = FreeCAD.activeDocument(). "
    "Solver CalculiX: solver = ObjectsFem.makeSolverCalculixCcxTools(doc, 'CalculiX'). "
    "solver.GeometricalNonlinearity = 'linear'; solver.ThermoMechSteadyState = False; "
    "solver.MatrixSolverType = 'default'; solver.IterationsControlParameterTimeUse = False. "
    "Aggiungi a analysis: analysis = doc.getObject('Analysis'); analysis.addObject(solver). "
    "Esegui: fea = ccxtools.FemToolsCcx(analysis=analysis, solver=solver, test_mode=False); "
    "fea.update_objects(); fea.setup_working_dir(); fea.setup_ccx(); "
    "message = fea.run(); print('Stato solver:', message if message else 'OK'). "
    "Errore se CalculiX non installato: sudo apt install calculix-ccx. "
    "doc.recompute(). Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    /* 9 — FEM Risultati (Von Mises, deformazione, temperatura) */
    "Sei un esperto di FreeCAD Python API e FEM risultati. Genera SOLO codice Python puro. "
    "import FreeCAD, FreeCADGui; doc = FreeCAD.activeDocument(). "
    "Accedi ai risultati: res = doc.getObjectsByLabel('CCX_Results')[0]. "
    "Von Mises stress: vm = res.vonMises; "
    "print(f'Max Von Mises: {max(vm):.2f} MPa, Min: {min(vm):.2f} MPa'). "
    "Deformazione: disp = res.DisplacementVectors; mags = [v.Length for v in disp]; "
    "print(f'Max spostamento: {max(mags):.4f} mm'). "
    "Principal stresses: s1 = res.MaxPrincipal; s3 = res.MinPrincipal. "
    "Visualizza nel viewer: FreeCADGui.ActiveDocument.getObject(res.Name).DisplayMode = 0. "
    "Esporta CSV: import csv, pathlib; "
    "out = pathlib.Path.home()/'Desktop'/'fem_results.csv'; "
    "with open(out,'w',newline='') as f: "
    "  w = csv.writer(f); w.writerow(['Node','VonMises','DispX','DispY','DispZ']); "
    "  [w.writerow([i, vm[i], disp[i].x, disp[i].y, disp[i].z]) for i in range(len(vm))]; "
    "print('Salvato:', out). "
    "doc.recompute(). Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    /* 10 — Materiale fisico FEM (acciaio, alluminio, titanio, compositi) */
    "Sei un esperto di FreeCAD Python API e FEM materiali. Genera SOLO codice Python puro. "
    "import FreeCAD, ObjectsFem; doc = FreeCAD.activeDocument() or FreeCAD.newDocument('Doc'). "
    "mat = ObjectsFem.makeMaterialSolid(doc, 'Materiale'). "
    "Materiali predefiniti (assegna a mat.Material): "
    "Acciaio S235: {'Name':'Steel','YoungsModulus':'210000 MPa','PoissonRatio':'0.30',"
    "'Density':'7900 kg/m^3','UltimateTensileStrength':'400 MPa','YieldStrength':'235 MPa',"
    "'ThermalConductivity':'54 W/m/K','ThermalExpansionCoefficient':'12e-6 1/K'}. "
    "Alluminio 6061-T6: {'Name':'Aluminum_6061','YoungsModulus':'68900 MPa','PoissonRatio':'0.33',"
    "'Density':'2700 kg/m^3','YieldStrength':'276 MPa','UltimateTensileStrength':'310 MPa'}. "
    "Acciaio inox 316L: {'Name':'Steel_316L','YoungsModulus':'193000 MPa','PoissonRatio':'0.29',"
    "'Density':'8000 kg/m^3','YieldStrength':'170 MPa'}. "
    "Titanio Ti-6Al-4V: {'Name':'Titanium_Ti6Al4V','YoungsModulus':'113800 MPa','PoissonRatio':'0.342',"
    "'Density':'4430 kg/m^3','YieldStrength':'880 MPa','UltimateTensileStrength':'950 MPa'}. "
    "CFRP (fibra carbonio): {'Name':'CFRP','YoungsModulus':'70000 MPa','PoissonRatio':'0.10',"
    "'Density':'1600 kg/m^3'}. "
    "Aggiungi ad analysis: analysis.addObject(mat). "
    "doc.recompute(). Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    /* 11 — Rendering FreeCAD (Render workbench / export per Blender) */
    "Sei un esperto di FreeCAD Python API e Render workbench. Genera SOLO codice Python puro. "
    "import FreeCAD, FreeCADGui; doc = FreeCAD.activeDocument(). "
    "Opzione A — Render workbench (se installato): "
    "  import Render; project = doc.addObject('Render::RenderProject','RenderProject'); "
    "  project.Renderer = 'Cycles'; "
    "  camera = doc.addObject('Render::Camera','RenderCamera'); "
    "  camera.Placement = FreeCAD.Placement(FreeCAD.Vector(0,-500,300), FreeCAD.Rotation(60,0,0)); "
    "  light = doc.addObject('Render::PointLight','Light1'); "
    "  light.Location = FreeCAD.Vector(200,200,500); light.Color = (1.0,1.0,1.0); light.Power = 60. "
    "  project.addObject(camera); project.addObject(light); "
    "  Render.Renderer.render(project). "
    "Opzione B — esporta GLTF per rendering in Blender: "
    "  import importGLTF; importGLTF.export(doc.Objects, '/tmp/freecad_model.gltf'). "
    "Opzione C — esporta OBJ+MTL: "
    "  import importOBJ; importOBJ.export(doc.Objects, '/tmp/freecad_model.obj'). "
    "doc.recompute(). Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    /* 12 — Importa modello (STEP / IGES / DXF / IFC / STL) */
    "Sei un esperto di FreeCAD Python API. Genera SOLO codice Python puro per importare geometria. "
    "import FreeCAD; doc = FreeCAD.activeDocument() or FreeCAD.newDocument('Import'). "
    "STEP: import Import; Import.insert('/path/file.step', doc.Name); doc.recompute(). "
    "IGES: Import.insert('/path/file.iges', doc.Name); doc.recompute(). "
    "DXF 2D: import importDXF; importDXF.insert('/path/file.dxf', doc.Name). "
    "IFC (BIM): import importIFC; importIFC.insert('/path/file.ifc', doc.Name). "
    "STL (come mesh): import Mesh; Mesh.insert('/path/file.stl', doc.Name). "
    "STL convertito in solido: import MeshPart; "
    "  solid = MeshPart.meshToShape(doc.getObject('MeshNome').Mesh, 1.0, True, True). "
    "SVG (curve 2D): import importSVG; importSVG.insert('/path/file.svg', doc.Name). "
    "OBJ: import importOBJ; importOBJ.insert('/path/file.obj', doc.Name). "
    "doc.recompute(). Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    /* 13 — Script libero */
    "Sei un esperto di FreeCAD Python API. Genera SOLO codice Python puro eseguibile in FreeCAD via exec(). "
    "Namespace disponibile: FreeCAD, FreeCADGui, Part, PartDesign, Sketcher, Draft, Mesh, Import, "
    "App, Gui, ObjectsFem, pathlib, os, math. "
    "Chiudi ogni script con doc.recompute(). "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    nullptr
};

static const char* kFreeCADActions[] = {
    "\xf0\x9f\x9f\xa6 Crea Primitiva",
    "\xe2\x9c\x8f Crea Schizzo",
    "\xe2\x9c\x82 Booleana",
    "\xf0\x9f\x93\xa4 Esporta STL/STEP",
    "\xf0\x9f\x94\xa7 Modifica propriet\xc3\xa0",
    "\xf0\x9f\x93\x90 Vincoli & misure",
    "\xf0\x9f\xa7\xb1 Part Design",
    "\xf0\x9f\x94\xac FEM Setup",
    "\xe2\x9a\x99 FEM Esegui",
    "\xf0\x9f\x93\x8a FEM Risultati",
    "\xf0\x9f\xa7\xaa Materiale fisico",
    "\xf0\x9f\x96\xbc Rendering",
    "\xf0\x9f\x93\xa5 Importa modello",
    "\xf0\x9f\x90\x8d Script libero",
    nullptr
};

/* ══════════════════════════════════════════════════════════════
   Tab FREECAD
   ══════════════════════════════════════════════════════════════ */
QWidget* AppControllerPage::buildFreeCADTab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);

    auto* descLbl = new QLabel(
        "\xf0\x9f\x94\xa9  <i>FreeCAD \xe2\x80\x94 Software CAD parametrico open-source per la progettazione "
        "meccanica e industriale 3D. Supporta modellazione solida, simulazione FEM e architettura BIM.</i>", w);
    descLbl->setObjectName("hintLabel");
    descLbl->setTextFormat(Qt::RichText);
    descLbl->setWordWrap(true);
    lay->addWidget(descLbl);

    /* ── Barra connessione ── */
    auto* connRow = new QWidget(w);
    auto* connLay = new QHBoxLayout(connRow);
    connLay->setContentsMargins(0, 0, 0, 0);
    connLay->setSpacing(8);

    auto* lbl = new QLabel(tr("FreeCAD:"), connRow);
    lbl->setObjectName("hintLabel");

    m_freecadHostEdit = new QLineEdit("localhost:9876", connRow);
    m_freecadHostEdit->setFixedWidth(dpiScale(150));

    auto* pingBtn = new QPushButton(tr("\xf0\x9f\x94\x97  Verifica"), connRow);
    pingBtn->setToolTip(tr("Verifica che FreeCAD sia in ascolto sulla porta specificata"));
    pingBtn->setObjectName("actionBtn");
    pingBtn->setFixedWidth(dpiScale(100));

    m_freecadStatusLbl = new QLabel(tr("\xe2\x9a\xaa  Non connesso"), connRow);
    m_freecadStatusLbl->setObjectName("hintLabel");

    m_freecadExecBtn = new QPushButton(
        tr("\xe2\x96\xb6  Esegui in FreeCAD"), connRow);
    m_freecadExecBtn->setObjectName("actionBtn");
    m_freecadExecBtn->setFixedWidth(dpiScale(160));
    m_freecadExecBtn->setEnabled(false);

    connLay->addWidget(lbl);
    connLay->addWidget(m_freecadHostEdit);
    connLay->addWidget(pingBtn);
    auto* freecadHelpBtn = new QPushButton(tr("\xf0\x9f\x9b\x9f  Aiuto"), connRow);
    freecadHelpBtn->setToolTip(tr("Apri la documentazione e i comandi AI disponibili per FreeCAD"));
    freecadHelpBtn->setObjectName("actionBtn");
    freecadHelpBtn->setFixedWidth(dpiScale(80));
    connLay->addWidget(m_freecadStatusLbl, 1);
    connLay->addWidget(m_freecadExecBtn);
    connLay->addWidget(freecadHelpBtn);
    lay->addWidget(connRow);

    /* ── Hint ── */
    auto* hintLbl = new QLabel(
        "\xf0\x9f\x93\xa6 <b>Bridge FreeCAD:</b> installa "
        "<a href='https://github.com/manuelbb-upb/FreeCAD-MCP'>"
        "FreeCAD-MCP</a> e avvia il server sulla porta 9876.", w);
    hintLbl->setObjectName("hintLabel");
    hintLbl->setOpenExternalLinks(true);
    hintLbl->setWordWrap(true);
    lay->addWidget(hintLbl);

    /* ── Azione + Modello ── */
    auto* toolRow = new QWidget(w);
    auto* toolLay = new QHBoxLayout(toolRow);
    toolLay->setContentsMargins(0, 0, 0, 0);
    toolLay->setSpacing(8);

    m_freecadAction = new QComboBox(toolRow);
    for (int i = 0; kFreeCADActions[i]; i++)
        m_freecadAction->addItem(QString::fromUtf8(kFreeCADActions[i]));

    m_freecadModel = new ModelComboBox(m_ai, toolRow);

    toolLay->addWidget(new QLabel(tr("Azione:"), toolRow));
    toolLay->addWidget(m_freecadAction, 1);
    toolLay->addWidget(new QLabel(tr("Modello:"), toolRow));
    toolLay->addWidget(m_freecadModel, 1);
    lay->addWidget(toolRow);

    /* ── Input ── */
    m_freecadInput = new QTextEdit(w);
    m_freecadInput->setPlaceholderText(
        tr("Descrivi cosa fare in FreeCAD — es.:\n"
        "  Crea un box 50x30x10 mm con foro passante da 6 mm\n"
        "  Analisi FEM statica: vincola Face1, forza 500 N su Face2, acciaio S235\n"
        "  Esegui CalculiX e mostra max Von Mises e spostamento\n"
        "  Aggiungi raccordo R2 su tutti gli spigoli verticali\n"
        "  Importa file STEP e assegna materiale alluminio 6061\n"
        "  Esporta rendering OBJ per Blender"));
    m_freecadInput->setMaximumHeight(dpiScale(80));
    m_freecadInput->setMinimumHeight(dpiScale(60));
    lay->addWidget(m_freecadInput);

    /* ── Run/Stop ── */
    auto* btnRow = new QWidget(w);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    btnLay->setSpacing(8);
    m_freecadRunBtn  = new QPushButton(tr("\xe2\x96\xb6  Genera codice AI"), btnRow);
    m_freecadRunBtn->setObjectName("actionBtn");
    m_freecadStopBtn = new QPushButton(tr("\xe2\x8f\xb9  Stop"), btnRow);
    m_freecadStopBtn->setObjectName("actionBtn");
    m_freecadStopBtn->setProperty("danger", true);
    m_freecadStopBtn->setEnabled(false);
    btnLay->addWidget(m_freecadRunBtn);
    btnLay->addWidget(m_freecadStopBtn);
    btnLay->addStretch();
    lay->addWidget(btnRow);

    /* ── Output ── */
    m_freecadOutput = new QTextEdit(w);
    m_freecadOutput->setReadOnly(true);
    m_freecadOutput->setObjectName("outputView");
    m_freecadOutput->setPlaceholderText(tr("Output AI e FreeCAD apparirà qui..."));
    lay->addWidget(m_freecadOutput, 1);

    /* ── Connessioni ── */
    connect(pingBtn,          &QPushButton::clicked,
            this, &AppControllerPage::onFreecadPingClicked);
    connect(m_freecadExecBtn, &QPushButton::clicked,
            this, &AppControllerPage::onFreecadExecClicked);
    connect(m_freecadRunBtn,  &QPushButton::clicked,
            this, &AppControllerPage::onFreecadRunClicked);
    connect(m_freecadStopBtn, &QPushButton::clicked,
            this, &AppControllerPage::onFreecadStopClicked);
    connect(freecadHelpBtn,   &QPushButton::clicked,
            this, &AppControllerPage::onFreecadHelpClicked);

    return w;
}

/* ======================================================================
   Sezione 4 — FreeCAD tab slots
   ====================================================================== */

void AppControllerPage::onFreecadPingClicked()
{
    QString addr = m_freecadHostEdit->text().trimmed();
    if (addr.isEmpty()) addr = "localhost:9876";
    const QString host = addr.contains(':') ? addr.section(':', 0, 0) : addr;
    const int     port = addr.contains(':') ? addr.section(':', 1).toInt() : 9876;
    m_freecadStatusLbl->setText(tr("\xf0\x9f\x94\x84  Connessione..."));
    if (m_freecadPingSock) { m_freecadPingSock->abort(); m_freecadPingSock->deleteLater(); }
    m_freecadPingSock = new QTcpSocket(this);
    connect(m_freecadPingSock, &QTcpSocket::connected,
            this, &AppControllerPage::onFreecadPingConnected);
    connect(m_freecadPingSock, &QAbstractSocket::errorOccurred,
            this, &AppControllerPage::onFreecadPingError);
    m_freecadPingSock->connectToHost(host, static_cast<quint16>(port));
    QTimer::singleShot(3000, this, &AppControllerPage::onFreecadPingTimeout);
}

void AppControllerPage::onFreecadPingConnected()
{
    if (!m_freecadPingSock) return;
    m_freecadPingSock->disconnectFromHost();
    m_freecadPingSock->deleteLater();
    m_freecadPingSock = nullptr;
    m_freecadStatusLbl->setText(tr("\xe2\x9c\x85  FreeCAD connesso"));
}

void AppControllerPage::onFreecadPingError(QAbstractSocket::SocketError)
{
    if (!m_freecadPingSock) return;
    const QString freecadPingErr = m_freecadPingSock->errorString();
    m_freecadStatusLbl->setText(tr("\xe2\x9d\x8c  ") + freecadPingErr);
    LogBus::post("\xe2\x9d\x8c FreeCAD: Ping errore: " + freecadPingErr);
    m_freecadPingSock->deleteLater();
    m_freecadPingSock = nullptr;
}

void AppControllerPage::onFreecadPingTimeout()
{
    if (m_freecadPingSock &&
        m_freecadPingSock->state() != QAbstractSocket::ConnectedState) {
        m_freecadStatusLbl->setText(tr("\xe2\x9d\x8c  Timeout"));
        LogBus::post("\xe2\x9d\x8c FreeCAD: Ping timeout.");
        m_freecadPingSock->abort();
        m_freecadPingSock->deleteLater();
        m_freecadPingSock = nullptr;
    }
}

void AppControllerPage::onFreecadExecClicked()
{
    if (m_freecadCode.isEmpty()) return;
    QString addr = m_freecadHostEdit->text().trimmed();
    if (addr.isEmpty()) addr = "localhost:9876";
    const QString host = addr.contains(':') ? addr.section(':', 0, 0) : addr;
    const int     port = addr.contains(':') ? addr.section(':', 1).toInt() : 9876;
    QJsonObject payload; payload["type"] = "run_script";
    QJsonObject params; params["script"] = m_freecadCode;
    payload["params"] = params;
    m_freecadExecBody = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    m_freecadExecBtn->setEnabled(false);
    m_freecadStatusLbl->setText(tr("\xf0\x9f\x94\x84  Invio a FreeCAD..."));
    if (m_freecadExecSock) { m_freecadExecSock->abort(); m_freecadExecSock->deleteLater(); }
    m_freecadExecSock = new QTcpSocket(this);
    connect(m_freecadExecSock, &QTcpSocket::connected,
            this, &AppControllerPage::onFreecadExecConnected);
    connect(m_freecadExecSock, &QTcpSocket::readyRead,
            this, &AppControllerPage::onFreecadExecReadyRead);
    connect(m_freecadExecSock, &QAbstractSocket::errorOccurred,
            this, &AppControllerPage::onFreecadExecError);
    m_freecadExecSock->connectToHost(host, static_cast<quint16>(port));
}

void AppControllerPage::onFreecadExecError(QAbstractSocket::SocketError)
{
    if (!m_freecadExecSock) return;
    const QString freecadExecErr = m_freecadExecSock->errorString();
    m_freecadStatusLbl->setText(tr("\xe2\x9d\x8c  ") + freecadExecErr);
    LogBus::post("\xe2\x9d\x8c FreeCAD: Exec errore: " + freecadExecErr);
    m_freecadExecBtn->setEnabled(true);
    m_freecadExecSock->deleteLater();
    m_freecadExecSock = nullptr;
}

void AppControllerPage::onFreecadExecConnected()
{
    if (!m_freecadExecSock) return;
    m_freecadExecSock->write(m_freecadExecBody);
    m_freecadExecSock->flush();
}

void AppControllerPage::onFreecadExecReadyRead()
{
    if (!m_freecadExecSock) return;
    const QByteArray data = m_freecadExecSock->readAll();
    m_freecadExecSock->disconnectFromHost();
    m_freecadExecSock->deleteLater();
    m_freecadExecSock = nullptr;
    m_freecadExecBtn->setEnabled(true);
    QJsonObject res = QJsonDocument::fromJson(data).object();
    const QString st = res["status"].toString();
    if (st == "ok" || st == "success") {
        m_freecadStatusLbl->setText(tr("\xe2\x9c\x85  Eseguito"));
        m_freecadOutput->append("\n\xe2\x9c\x85  FreeCAD: "
            + res["result"].toString("OK"));
    } else {
        const QString freecadResErr = res["message"].toString();
        m_freecadStatusLbl->setText(tr("\xe2\x9d\x8c  ") + freecadResErr);
        m_freecadOutput->append("\n\xe2\x9d\x8c  FreeCAD errore: " + freecadResErr);
        LogBus::post("\xe2\x9d\x8c FreeCAD: Errore risposta: " + freecadResErr);
    }
}

void AppControllerPage::onFreecadRunClicked()
{
    const int idx = m_freecadAction->currentIndex();
    if (idx < 0 || !kFreeCADSys[idx]) return;
    runAi(1, QString::fromUtf8(kFreeCADSys[idx]),
          m_freecadInput->toPlainText(),
          m_freecadOutput, m_freecadRunBtn, m_freecadStopBtn,
          m_freecadModel);
}

void AppControllerPage::onFreecadStopClicked()
{
    m_ai->abort();
    m_freecadRunBtn->setEnabled(true);
    m_freecadStopBtn->setEnabled(false);
    m_freecadOutput->append("\n\xe2\x8f\xb9  Fermato.");
}

void AppControllerPage::onFreecadHelpClicked()
{
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(tr("\xf0\x9f\x94\xa9  Installazione FreeCAD MCP"));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(540, 440);
    auto* dlay    = new QVBoxLayout(dlg);
    auto* browser = new QTextBrowser(dlg);
    browser->setOpenExternalLinks(true);
    browser->setHtml(
        "<h3>\xf0\x9f\x94\xa9 FreeCAD MCP</h3>"
        "<h4>1. Installa FreeCAD</h4>"
        "<p><code>sudo apt install freecad</code> oppure "
        "<a href='https://www.freecad.org/downloads.php'>freecad.org</a></p>"
        "<h4>2. Clona il bridge</h4>"
        "<pre>git clone https://github.com/manuelbb-upb/FreeCAD-MCP</pre>"
        "<p>Segui il README per installare il modulo addon in FreeCAD.</p>"
        "<h4>3. Avvia il server</h4>"
        "<p>FreeCAD \xe2\x86\x92 Strumenti \xe2\x86\x92 Macro \xe2\x86\x92 esegui lo script bridge "
        "(porta <b>9876</b>)</p>"
        "<h4>4. Collega</h4>"
        "<p>Torna qui \xe2\x86\x92 clicca <b>\xf0\x9f\x94\x97 Verifica</b>.</p>");
    auto* btnClose = new QPushButton(tr("\xe2\x9c\x95  Chiudi"), dlg);
    btnClose->setObjectName("actionBtn");
    connect(btnClose, &QPushButton::clicked, dlg, &QDialog::accept);
    dlay->addWidget(browser);
    dlay->addWidget(btnClose);
    dlg->exec();
}

