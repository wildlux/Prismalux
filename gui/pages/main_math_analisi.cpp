/* main_math_analisi.cpp — Analisi 1&2: UI + slot + KaTeX */
#include "main_math.h"
#include "../prismalux_paths.h"
#include "../log_bus.h"
#include "../ai_client.h"
#include "../dpi_utils.h"
#include "../widgets/formula_parser.h"
#include "main_graph.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QProcess>
#include <QTextCursor>
#include <QScrollBar>
#include <QClipboard>
#include <QApplication>
#include <QTimer>
#include <QToolButton>
#include <QMenu>
#include <QScrollArea>
#include <QStackedWidget>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTextBrowser>
#include <QMessageBox>
#include <QFileDialog>
#include <QDir>
#include <QFile>
#include <cmath>
#include <limits>

namespace P = PrismaluxPaths;

struct AnalisiTopic { const char* name; const char* html; const char* ex; const char* type; const char* plotEx; };

static const AnalisiTopic kA1[] = {
  { "Limiti",
    "<h3 style='color:#60a5fa'>Limiti di funzione</h3>"
    "<p><b>Definizione &epsilon;&ndash;&delta;:</b></p>"
    "\\[\\lim_{x\\to c} f(x) = L \\iff "
    "\\forall\\varepsilon>0\\;\\exists\\delta>0:\\;"
    "0<|x-c|<\\delta \\Rightarrow |f(x)-L|<\\varepsilon\\]"
    "<p><b>Limiti notevoli (\\(x\\to 0\\)):</b></p>"
    "<ul>"
    "<li>\\(\\dfrac{\\sin x}{x}\\to 1\\) &nbsp;&nbsp; "
    "\\(\\dfrac{1-\\cos x}{x^2}\\to\\dfrac{1}{2}\\)</li>"
    "<li>\\(\\dfrac{\\ln(1+x)}{x}\\to 1\\) &nbsp;&nbsp; "
    "\\(\\dfrac{e^x-1}{x}\\to 1\\)</li>"
    "<li>\\(\\dfrac{\\arctan x}{x}\\to 1\\) &nbsp;&nbsp; "
    "\\(\\dfrac{a^x-1}{x}\\to \\ln a\\)</li>"
    "<li>Per \\(x\\to\\infty\\): &nbsp; "
    "\\(\\left(1+\\dfrac{1}{x}\\right)^x\\to e\\)</li>"
    "</ul><hr>"
    "<div class='box-purple'>"
    "<b style='color:#c4b5fd'>&#x2728; Regola di L&rsquo;H&ocirc;pital</b> "
    "(forme \\(0/0\\) o \\(\\infty/\\infty\\)):"
    "\\[\\lim\\frac{f}{g}=\\lim\\frac{f'}{g'}\\]"
    "si applica iterativamente se la forma rimane indeterminata.</div>"
    "<div class='box-green'>"
    "<b style='color:#86efac'>&#x221e; Gerarchia degli infiniti</b> "
    "per \\(x\\to+\\infty\\):"
    "\\[\\log x \\ll x^{\\alpha} \\ll e^x \\ll x^x \\quad(\\alpha>0)\\]"
    "<small>Utile per limiti \\(\\infty-\\infty\\).</small></div>"
    "<div class='box-blue'><b style='color:#93c5fd'>Esempi SymPy (tipo Limite):</b><br>"
    "sin(x)/x, x, 0 &bull; (exp(x)-1)/x, x, 0<br>"
    "x*log(x), x, 0 &bull; (1+1/x)**x, x, oo</div>",
    "sin(x)/x, x, 0", "Limite", "sin(x)/x" },

  { "Derivate",
    "<h3 style='color:#60a5fa'>Derivate</h3>"
    "<p><b>Definizione:</b>"
    "\\[f'(x)=\\lim_{h\\to 0}\\frac{f(x+h)-f(x)}{h}\\]</p>"
    "<p><b>Tavola derivate fondamentali:</b></p>"
    "<ul>"
    "<li>\\(D[x^n]=nx^{n-1}\\) &bull; \\(D[e^x]=e^x\\) &bull; "
    "\\(D[\\ln x]=\\dfrac{1}{x}\\)</li>"
    "<li>\\(D[\\sin x]=\\cos x\\) &bull; \\(D[\\cos x]=-\\sin x\\)</li>"
    "<li>\\(D[\\tan x]=\\dfrac{1}{\\cos^2 x}\\) &bull; "
    "\\(D[\\arcsin x]=\\dfrac{1}{\\sqrt{1-x^2}}\\)</li>"
    "</ul><hr>"
    "<p><b>Regole:</b></p><ul>"
    "<li>\\((fg)'=f'g+fg'\\)</li>"
    "<li>\\(\\left(\\dfrac{f}{g}\\right)'=\\dfrac{f'g-fg'}{g^2}\\)</li>"
    "<li>\\((f\\circ g)'=(f'\\circ g)\\cdot g'\\) (catena)</li>"
    "</ul><hr>"
    "<p><b>Rolle:</b> \\(f(a)=f(b)\\Rightarrow\\exists c: f'(c)=0\\)<br>"
    "<b>Lagrange:</b> \\(\\exists c\\in(a,b):\\;"
    "f'(c)=\\dfrac{f(b)-f(a)}{b-a}\\)</p>"
    "<div class='box-blue'><b style='color:#93c5fd'>Esempi SymPy (tipo Derivata):</b><br>"
    "x**3 - 2*x + 1, x &bull; sin(x)*exp(x), x<br>"
    "log(x**2+1), x &bull; x**2*cos(x), x, 2</div>",
    "x**3 - 2*x + 1, x", "Derivata", "x**3 - 2*x + 1" },

  { "Integrali indefiniti",
    "<h3 style='color:#60a5fa'>Integrali indefiniti</h3>"
    "<p><b>Definizione:</b> \\(F'(x)=f(x)\\Rightarrow"
    "\\int f(x)\\,dx=F(x)+C\\)</p><hr>"
    "<p><b>Integrali immediati:</b></p><ul>"
    "<li>\\(\\displaystyle\\int x^n\\,dx=\\dfrac{x^{n+1}}{n+1}+C\\quad(n\\ne -1)\\)</li>"
    "<li>\\(\\displaystyle\\int e^x\\,dx=e^x+C\\) &bull; "
    "\\(\\displaystyle\\int\\sin x\\,dx=-\\cos x+C\\)</li>"
    "<li>\\(\\displaystyle\\int\\dfrac{1}{x}\\,dx=\\ln|x|+C\\) &bull; "
    "\\(\\displaystyle\\int\\dfrac{1}{1+x^2}\\,dx=\\arctan x+C\\)</li>"
    "</ul><hr>"
    "<p><b>Tecniche:</b></p><ul>"
    "<li><b>Sostituzione:</b> \\(\\int f(g(x))g'(x)\\,dx\\;\\to\\; t=g(x)\\)</li>"
    "<li><b>Per parti:</b> \\(\\int u\\,dv=uv-\\int v\\,du\\)</li>"
    "<li><b>Frazioni parziali:</b> \\(\\dfrac{p(x)}{q(x)}\\) con \\(q\\) fattorizzabile</li>"
    "</ul>"
    "<div class='box-blue'><b style='color:#93c5fd'>Esempi SymPy (tipo Integrale):</b><br>"
    "x**2*exp(x), x &bull; sin(x)**2, x<br>"
    "1/(x**2-1), x &bull; log(x), x</div>",
    "x**2*exp(x), x", "Integrale", "x**2*exp(x)" },

  { "Integrali definiti",
    "<h3 style='color:#60a5fa'>Integrali definiti</h3>"
    "<p><b>Def. (somme di Riemann):</b>"
    "\\[\\int_a^b f(x)\\,dx=\\lim_{n\\to\\infty}\\sum_k f(x_k)\\Delta x\\]</p>"
    "<p><b>Teorema fondamentale del calcolo:</b>"
    "\\[\\int_a^b f(x)\\,dx=F(b)-F(a)\\quad\\text{dove }F'=f\\]</p>"
    "<p><b>Propriet&agrave;:</b> linearit&agrave;, additivit&agrave; sugli intervalli, monotonia.<br>"
    "<b>Teorema della media:</b> "
    "\\(\\exists c\\in(a,b):\\int_a^b f\\,dx=f(c)(b-a)\\)</p>"
    "<p><b>Applicazioni:</b> area, lunghezza arco, volumi di rotazione.</p>"
    "<div class='box-blue'><b style='color:#93c5fd'>Esempi SymPy (tipo Integrale):</b><br>"
    "sin(x), x, 0, pi &bull; x**2, x, 0, 1<br>"
    "exp(-x**2), x, 0, 1 &bull; 1/x, x, 1, exp(1)</div>",
    "sin(x), x, 0, pi", "Integrale", "sin(x)" },

  { "Studio di funzione",
    "<h3 style='color:#60a5fa'>Studio di funzione</h3>"
    "<p><b>Schema completo:</b></p>"
    "<ol>"
    "<li><b>Dominio</b> &mdash; dove \\(f(x)\\) &egrave; definita</li>"
    "<li><b>Simmetrie</b> &mdash; pari (\\(f(-x)=f(x)\\)), dispari</li>"
    "<li><b>Limiti agli estremi</b> &mdash; asintoti orizzontali/verticali</li>"
    "<li><b>Segno</b> &mdash; \\(f(x)>0\\), \\(f(x)=0\\)</li>"
    "<li><b>Monotonia</b> &mdash; \\(f'(x)>0\\) crescente, \\(<0\\) decrescente</li>"
    "<li><b>Estremi relativi</b> &mdash; \\(f'(x_0)=0\\Rightarrow f''\\) decide</li>"
    "<li><b>Convessit&agrave;</b> &mdash; \\(f''>0\\) conv. \\(\\uparrow\\), "
    "\\(f''<0\\) conv. \\(\\downarrow\\)</li>"
    "<li><b>Asintoto obliquo:</b> \\(y=mx+q\\), "
    "\\(m=\\lim\\dfrac{f(x)}{x}\\), \\(q=\\lim[f(x)-mx]\\)</li>"
    "</ol>"
    "<div class='box-blue'><b style='color:#93c5fd'>Esempi SymPy (tipo Derivata):</b><br>"
    "(x**2-1)/(x**2+1), x &bull; x*exp(-x), x<br>"
    "x**3-3*x, x &bull; log(x)/x, x</div>",
    "(x**2-1)/(x**2+1), x", "Derivata", "(x**2-1)/(x**2+1)" },

  { "Serie di Taylor",
    "<h3 style='color:#60a5fa'>Serie di Taylor &amp; Maclaurin</h3>"
    "<p><b>Formula di Taylor in \\(x_0\\):</b>"
    "\\[f(x)=\\sum_{k=0}^{n}\\frac{f^{(k)}(x_0)}{k!}(x-x_0)^k + R_n(x)\\]</p>"
    "<p><b>Sviluppi di Maclaurin fondamentali (in 0):</b></p><ul>"
    "<li>\\(e^x=1+x+\\dfrac{x^2}{2!}+\\dfrac{x^3}{3!}+\\cdots\\)</li>"
    "<li>\\(\\sin x=x-\\dfrac{x^3}{3!}+\\dfrac{x^5}{5!}-\\cdots\\)</li>"
    "<li>\\(\\cos x=1-\\dfrac{x^2}{2!}+\\dfrac{x^4}{4!}-\\cdots\\)</li>"
    "<li>\\(\\ln(1+x)=x-\\dfrac{x^2}{2}+\\dfrac{x^3}{3}-\\cdots\\quad|x|<1\\)</li>"
    "<li>\\((1+x)^\\alpha=1+\\alpha x+"
    "\\dfrac{\\alpha(\\alpha-1)}{2!}x^2+\\cdots\\)</li>"
    "</ul>"
    "<div class='box-blue'><b style='color:#93c5fd'>Esempi SymPy (tipo Semplificazione):</b><br>"
    "series(exp(x), x, 0, 6) &rarr; usa il campo Espressione<br>"
    "series(sin(x), x, 0, 8)<br>"
    "series(log(1+x), x, 0, 5)</div>",
    "series(exp(x), x, 0, 6)", "Semplificazione", "exp(x)" },

  { "Successioni e serie numeriche",
    "<h3 style='color:#60a5fa'>Successioni e serie</h3>"
    "<p><b>Successione</b> \\(\\{a_n\\}\\): converge a \\(L\\) se "
    "\\(|a_n-L|\\to 0\\)</p>"
    "<p><b>Serie</b> \\(\\sum a_n\\): \\(S_n=a_1+\\cdots+a_n\\), "
    "converge se \\(S_n\\) ha limite finito</p><hr>"
    "<p><b>Criteri di convergenza:</b></p><ul>"
    "<li><b>Confronto:</b> \\(0\\le a_n\\le b_n\\); "
    "\\(b_n\\) conv. \\(\\Rightarrow a_n\\) conv.</li>"
    "<li><b>Rapporto (D&rsquo;Alembert):</b> "
    "\\(\\rho=\\lim\\left|\\dfrac{a_{n+1}}{a_n}\\right|\\); "
    "\\(\\rho<1\\) conv.</li>"
    "<li><b>Radice (Cauchy):</b> "
    "\\(\\rho=\\lim\\sqrt[n]{|a_n|}\\); \\(\\rho<1\\) conv.</li>"
    "<li><b>Leibniz:</b> \\(\\sum(-1)^n a_n\\) conv. se \\(a_n\\downarrow 0\\)</li>"
    "</ul>"
    "<p><b>Serie geometrica:</b> "
    "\\(\\displaystyle\\sum q^n=\\dfrac{1}{1-q}\\) per \\(|q|<1\\)</p>"
    "<div class='box-blue'><b style='color:#93c5fd'>Esempi SymPy (tipo Semplificazione):</b><br>"
    "Sum(1/n**2, (n,1,oo)) &rarr; usa Espressione<br>"
    "Sum((-1)**n/factorial(n), (n,0,oo))</div>",
    "Sum(1/n**2, (n,1,oo))", "Semplificazione", "1/x**2" },
};

static const AnalisiTopic kA2[] = {
  { "Derivate parziali",
    "<h3 style='color:#fb923c'>Derivate parziali</h3>"
    "<p><b>Definizione:</b>"
    "\\[\\frac{\\partial f}{\\partial x}(x_0,y_0)="
    "\\lim_{h\\to 0}\\frac{f(x_0+h,y_0)-f(x_0,y_0)}{h}\\]</p>"
    "<p><b>Differenziale totale:</b>"
    "\\[df=\\frac{\\partial f}{\\partial x}\\,dx+"
    "\\frac{\\partial f}{\\partial y}\\,dy\\]</p>"
    "<p>\\(f\\) differenziabile \\(\\Rightarrow\\) derivate parziali continue</p><hr>"
    "<p><b>Regola della catena:</b> \\(z=f(x(t),y(t))\\)"
    "\\[\\frac{dz}{dt}=\\frac{\\partial f}{\\partial x}\\frac{dx}{dt}+"
    "\\frac{\\partial f}{\\partial y}\\frac{dy}{dt}\\]</p>"
    "<div class='box-orange'><b style='color:#93c5fd'>Esempi SymPy (tipo Derivata):</b><br>"
    "x**2*y + sin(x*y), x &rarr; deriv. parz. in x<br>"
    "exp(x**2+y**2), y &bull; x**3*y**2, x, 2</div>",
    "x**2*y + sin(x*y), x", "Derivata", "x**2*y + sin(x*y)" },

  { "Gradiente e piano tangente",
    "<h3 style='color:#fb923c'>Gradiente e piano tangente</h3>"
    "<p><b>Gradiente</b> di \\(f(x,y)\\):"
    "\\[\\nabla f=\\left(\\frac{\\partial f}{\\partial x},"
    "\\frac{\\partial f}{\\partial y}\\right)\\]</p>"
    "<p><b>Derivata direzionale:</b> "
    "\\(D_{\\hat{u}}f=\\nabla f\\cdot\\hat{u}\\)<br>"
    "Massima crescita = direzione di \\(\\nabla f\\)</p><hr>"
    "<p><b>Piano tangente</b> a \\(z=f(x,y)\\) in \\((x_0,y_0)\\):"
    "\\[z=f(x_0,y_0)+f_x(x_0,y_0)(x-x_0)+f_y(x_0,y_0)(y-y_0)\\]</p>"
    "<div class='box-orange'><b style='color:#93c5fd'>Esempi SymPy (tipo Derivata):</b><br>"
    "x**2 + y**2, x &bull; x*y*exp(x+y), y<br>"
    "sin(x)*cos(y), x</div>",
    "x**2 + y**2, x", "Derivata", "x**2 + y**2" },

  { "Hessiana ed estremi liberi",
    "<h3 style='color:#fb923c'>Matrice Hessiana ed estremi</h3>"
    "<p><b>Matrice Hessiana</b> \\(H_f\\) in \\((x_0,y_0)\\):"
    "\\[H=\\begin{pmatrix}f_{xx}&amp;f_{xy}\\\\"
    "f_{yx}&amp;f_{yy}\\end{pmatrix}\\]</p>"
    "<p><b>Punti critici:</b> \\(\\nabla f=0\\) "
    "(\\(f_x=0,\\;f_y=0\\))</p>"
    "<p><b>Criterio dell&rsquo;Hessiana:</b></p><ul>"
    "<li>\\(\\det(H)>0\\) e \\(f_{xx}>0 \\Rightarrow\\) <b>minimo</b></li>"
    "<li>\\(\\det(H)>0\\) e \\(f_{xx}<0 \\Rightarrow\\) <b>massimo</b></li>"
    "<li>\\(\\det(H)<0 \\Rightarrow\\) <b>punto di sella</b></li>"
    "<li>\\(\\det(H)=0 \\Rightarrow\\) indecidibile</li>"
    "</ul>"
    "<div class='box-orange'><b style='color:#93c5fd'>Esempi SymPy (Semplificazione):</b><br>"
    "usa Espressione: hessian(x**4+y**4-4*x*y, [x,y])<br>"
    "oppure: solve([diff(x**3+y**3-3*x*y,x), diff(x**3+y**3-3*x*y,y)])</div>",
    "x**4 + y**4 - 4*x*y", "Semplificazione", "x**4 + y**4 - 4*x*y" },

  { "Moltiplicatori di Lagrange",
    "<h3 style='color:#fb923c'>Moltiplicatori di Lagrange</h3>"
    "<p><b>Problema:</b> max/min \\(f(x,y)\\) soggetto a \\(g(x,y)=0\\)</p>"
    "<p><b>Condizione necessaria</b> in un punto estremo \\(P_0\\):"
    "\\[\\nabla f(P_0)=\\lambda\\,\\nabla g(P_0)\\]</p>"
    "<p><b>Sistema da risolvere:</b>"
    "\\[f_x=\\lambda g_x,\\quad f_y=\\lambda g_y,\\quad g(x,y)=0\\]</p>"
    "<p>Estensione a pi&ugrave; variabili:"
    "\\[\\nabla f=\\lambda_1\\nabla g_1+\\lambda_2\\nabla g_2+\\cdots\\]</p>"
    "<div class='box-orange'><b>SymPy (tab Espressione):</b><br>"
    "solve([diff(x**2+y**2,x)-lam*diff(x+y-1,x),<br>"
    "&nbsp;diff(x**2+y**2,y)-lam*diff(x+y-1,y),x+y-1],[x,y,lam])</div>",
    "x**2 + y**2", "Semplificazione", "x**2 + y**2" },

  { "Integrali doppi",
    "<h3 style='color:#fb923c'>Integrali doppi</h3>"
    "<p><b>Teorema di Fubini</b> su \\([a,b]\\times[c,d]\\):"
    "\\[\\iint_D f\\,dA=\\int_a^b\\left[\\int_c^d f(x,y)\\,dy\\right]dx\\]</p>"
    "<p><b>Dominio normale</b> "
    "(\\(a\\le x\\le b\\), \\(\\varphi_1\\le y\\le\\varphi_2\\)):"
    "\\[\\iint f\\,dA=\\int_a^b\\int_{\\varphi_1(x)}^{\\varphi_2(x)}"
    "f(x,y)\\,dy\\,dx\\]</p>"
    "<p><b>Coordinate polari:</b> \\(x=r\\cos\\theta\\), "
    "\\(y=r\\sin\\theta\\)"
    "\\[\\iint f\\,dA=\\iint f(r\\cos\\theta,r\\sin\\theta)"
    "\\cdot r\\,dr\\,d\\theta\\]</p>"
    "<div class='box-orange'><b style='color:#93c5fd'>Esempi SymPy (Integrale):</b><br>"
    "x*y, x, 0, 1 (semplice)<br>"
    "Tab Espressione: integrate(integrate(x*y,(y,0,x)),(x,0,1))</div>",
    "integrate(x*y, (y, 0, x))", "Semplificazione", "x*y" },

  { "Equazioni differenziali",
    "<h3 style='color:#fb923c'>Equazioni differenziali ordinarie</h3>"
    "<p><b>EDO I ordine separabile:</b> \\(y'=f(x)g(y)\\)"
    "\\[\\int\\frac{dy}{g(y)}=\\int f(x)\\,dx\\]</p>"
    "<p><b>EDO I ordine lineare:</b> \\(y'+p(x)y=q(x)\\)<br>"
    "Fattore integrante: \\(\\mu(x)=e^{\\int p\\,dx}\\)"
    "\\[y=\\frac{\\int\\mu(x)q(x)\\,dx+C}{\\mu(x)}\\]</p>"
    "<p><b>EDO II ordine a coeff. costanti:</b> \\(ay''+by'+cy=0\\)<br>"
    "Eq. caratteristica: \\(a\\lambda^2+b\\lambda+c=0\\)</p><ul>"
    "<li>\\(\\Delta>0\\): \\(y=C_1 e^{\\lambda_1 x}+C_2 e^{\\lambda_2 x}\\)</li>"
    "<li>\\(\\Delta=0\\): \\(y=(C_1+C_2 x)e^{\\lambda x}\\)</li>"
    "<li>\\(\\Delta<0\\): "
    "\\(y=e^{\\alpha x}(C_1\\cos\\beta x+C_2\\sin\\beta x)\\)</li>"
    "</ul>"
    "<div class='box-orange'><b>SymPy (tab Espressione):</b><br>"
    "dsolve(f(x).diff(x) + f(x) - exp(x), f(x))<br>"
    "dsolve(f(x).diff(x,2)+f(x), f(x))</div>",
    "dsolve(f(x).diff(x) + f(x) - exp(x), f(x))", "Semplificazione", "exp(-x) + exp(x)/2" },

  { "Calcolo vettoriale",
    "<h3 style='color:#fb923c'>Calcolo vettoriale</h3>"
    "<p><b>Divergenza</b> di \\(\\mathbf{F}=(P,Q,R)\\):"
    "\\[\\operatorname{div}\\mathbf{F}="
    "\\frac{\\partial P}{\\partial x}+"
    "\\frac{\\partial Q}{\\partial y}+"
    "\\frac{\\partial R}{\\partial z}\\]</p>"
    "<p><b>Rotore (curl):</b>"
    "\\[\\operatorname{rot}\\mathbf{F}=\\nabla\\times\\mathbf{F}="
    "\\begin{vmatrix}\\mathbf{i}&amp;\\mathbf{j}&amp;\\mathbf{k}\\\\"
    "\\partial_x&amp;\\partial_y&amp;\\partial_z\\\\"
    "P&amp;Q&amp;R\\end{vmatrix}\\]</p>"
    "<p><b>Teorema di Gauss:</b>"
    "\\[\\iint_{\\partial V}\\mathbf{F}\\cdot\\mathbf{n}\\,dS="
    "\\iiint_V\\operatorname{div}\\mathbf{F}\\,dV\\]</p>"
    "<p><b>Teorema di Stokes:</b>"
    "\\[\\oint_{\\partial\\Sigma}\\mathbf{F}\\cdot d\\mathbf{r}="
    "\\iint_\\Sigma\\operatorname{rot}\\mathbf{F}\\cdot\\mathbf{n}\\,dS\\]</p>"
    "<p><b>Potenziale:</b> \\(\\operatorname{rot}\\mathbf{F}=0"
    "\\iff\\mathbf{F}=\\nabla\\varphi\\)</p>"
    "<div class='box-orange'><b>SymPy (tab Espressione):</b><br>"
    "from sympy.vector import CoordSys3D; N=CoordSys3D('N')<br>"
    "F = N.x**2*N.i + N.y**2*N.j; divergence(F)</div>",
    "x**2 + y**2 + z**2", "Semplificazione", "x**2 + y**2" },
};


/* ══════════════════════════════════════════════════════════════
   Helper — crea combo tipo identica a m_solveCmb (sottoinsieme)
   ══════════════════════════════════════════════════════════════ */
static QComboBox* makeAnalisiTypeCmb(QWidget* parent)
{
    auto* c = new QComboBox(parent);
    c->setObjectName("settingCombo");
    c->addItem("Derivata",        "Derivata");
    c->addItem("Integrale",       "Integrale");
    c->addItem("Limite",          "Limite");
    c->addItem("Semplificazione", "Semplificazione");
    c->addItem("Equazione",       "Equazione");
    return c;
}


/* Costruisce Analisi 1/2 al primo accesso reale (clic utente o setCurrentIndex
   esterno) invece che nel costruttore — vedi commento su m_analisi1Idx in
   main_math.h per il motivo (freeze da inizializzazione Chromium/LatexView). */
void MatematicaPage::ensureAnalisiTabBuilt(int idx)
{
    if (idx == m_analisi1Idx && !m_analisi1Built) {
        m_analisi1Built = true;
        QWidget* placeholder = m_tabs->widget(idx);
        QWidget* real = buildAnalisi1Tab();
        m_tabs->removeTab(idx);
        m_tabs->insertTab(idx, real, "\xf0\x9f\x93\x98  Analisi 1");
        m_tabs->setCurrentIndex(idx);
        placeholder->deleteLater();
    } else if (idx == m_analisi2Idx && !m_analisi2Built) {
        m_analisi2Built = true;
        QWidget* placeholder = m_tabs->widget(idx);
        QWidget* real = buildAnalisi2Tab();
        m_tabs->removeTab(idx);
        m_tabs->insertTab(idx, real, "\xf0\x9f\x93\x99  Analisi 2");
        m_tabs->setCurrentIndex(idx);
        placeholder->deleteLater();
    }
}

QWidget* MatematicaPage::buildAnalisi1Tab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(10, 8, 10, 8);
    lay->setSpacing(8);

    /* ─── Riga selettore argomento ─── */
    auto* topRow = new QHBoxLayout;
    auto* topLbl = new QLabel(tr("<b>\xf0\x9f\x93\x98  Argomento:</b>"), w);
    topRow->addWidget(topLbl);

    m_a1TopicCmb = new QComboBox(w);
    m_a1TopicCmb->setObjectName("settingCombo");
    const int nA1 = static_cast<int>(sizeof(kA1)/sizeof(kA1[0]));
    for (int i = 0; i < nA1; ++i)
        m_a1TopicCmb->addItem(QString::fromUtf8(kA1[i].name), i);
    topRow->addWidget(m_a1TopicCmb, 1);

    auto* btnAiExplain = new QPushButton(
        tr("\xf0\x9f\xa4\x96  Spiega con AI"), w);
    btnAiExplain->setObjectName("actionBtn");
    btnAiExplain->setProperty("analisiLevel", 1);
    connect(btnAiExplain, &QPushButton::clicked, this, &MatematicaPage::onA1AiClicked);
    topRow->addWidget(btnAiExplain);
    lay->addLayout(topRow);

    /* ─── Splitter orizzontale: Teoria (sx) | Canvas interattivo (dx) ─── */
    auto* hSplit = new QSplitter(Qt::Horizontal, w);
    hSplit->setHandleWidth(5);

    m_a1Theory = new LatexView(hSplit);
    m_a1Theory->setMinimumWidth(180);
    hSplit->addWidget(m_a1Theory);

    m_a1Canvas = new GraficoCanvas(hSplit);
    m_a1Canvas->setMinimumWidth(180);
    hSplit->addWidget(m_a1Canvas);

    hSplit->setStretchFactor(0, 3);  /* teoria */
    hSplit->setStretchFactor(1, 4);  /* canvas */
    lay->addWidget(hSplit, 1);

    /* ─── Riga Risolvi ─── */
    auto* tryRow = new QHBoxLayout;
    tryRow->addWidget(new QLabel(tr("\xf0\x9f\x93\x90  Risolvi:"), w));
    m_a1Input = new QLineEdit(w);
    m_a1Input->setPlaceholderText(tr("Espressione SymPy (pre-compilata dall'argomento)"));
    tryRow->addWidget(m_a1Input, 1);
    m_a1TypeCmb = makeAnalisiTypeCmb(w);
    tryRow->addWidget(m_a1TypeCmb);
    auto* btnTry = new QPushButton(
        tr("\xf0\x9f\x93\x90  Risolvi nella scheda accanto"), w);
    btnTry->setObjectName("actionBtn");
    btnTry->setProperty("highlight", "true");
    btnTry->setToolTip(
        "Copia la formula nella scheda \xe2\x80\x9c\xf0\x9f\x93\x90 Risolvi Passi\xe2\x80\x9d "
        "e avvia il calcolo SymPy passo per passo.");
    connect(btnTry, &QPushButton::clicked, this, &MatematicaPage::onA1TryClicked);
    tryRow->addWidget(btnTry);

    auto* btnQuickPlot1 = new QPushButton(
        tr("\xf0\x9f\x93\x88  Disegna grafico"), w);
    btnQuickPlot1->setObjectName("actionBtn");
    btnQuickPlot1->setToolTip(tr("Copia l'espressione nel campo Grafico e traccia subito"));
    connect(btnQuickPlot1, &QPushButton::clicked, this, [this]() {
        if (m_a1Input && m_a1PlotInput)
            m_a1PlotInput->setText(m_a1Input->text());
        onA1PlotClicked();
    });
    tryRow->addWidget(btnQuickPlot1);
    lay->addLayout(tryRow);

    /* ─── Riga Grafico ─── */
    auto* plotRow = new QHBoxLayout;
    plotRow->addWidget(new QLabel(tr("\xf0\x9f\x93\x88  Grafico:"), w));
    m_a1PlotInput = new QLineEdit(w);
    m_a1PlotInput->setPlaceholderText(tr("f(x) da disegnare nel canvas"));
    plotRow->addWidget(m_a1PlotInput, 1);
    auto* btnPlot1 = new QPushButton(
        tr("\xf0\x9f\x93\x88  Disegna grafico"), w);
    btnPlot1->setObjectName("actionBtn");
    btnPlot1->setToolTip(tr("Traccia f(x) nel canvas a destra"));
    connect(btnPlot1, &QPushButton::clicked, this, &MatematicaPage::onA1PlotClicked);
    plotRow->addWidget(btnPlot1);
    m_a1RenderCmb = new QComboBox(w);
    m_a1RenderCmb->addItem("\xe2\x80\x94  Linea",   0);
    m_a1RenderCmb->addItem("\xe2\x97\x8f  Punti",   1);
    m_a1RenderCmb->addItem("\xe2\x96\xb2  Area",    2);
    m_a1RenderCmb->setToolTip(tr("Stile rendering 2D"));
    m_a1RenderCmb->setMaximumWidth(110);
    connect(m_a1RenderCmb, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MatematicaPage::onA1RenderChanged);
    plotRow->addWidget(m_a1RenderCmb);
    m_btnA1Expand = new QPushButton("\xe2\x86\x97", w);   /* ↗ */
    m_btnA1Expand->setToolTip(tr("Apri grafico in finestra separata"));
    m_btnA1Expand->setFixedWidth(dpiScale(32));
    connect(m_btnA1Expand, &QPushButton::clicked, this, &MatematicaPage::onA1ExpandClicked);
    plotRow->addWidget(m_btnA1Expand);
    lay->addLayout(plotRow);

    connect(m_a1TopicCmb, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MatematicaPage::onA1TopicChanged);
    onA1TopicChanged();   /* carica il primo argomento */

    return w;
}

/* ══════════════════════════════════════════════════════════════
   buildAnalisi2Tab — studio funzioni Analisi 2
   ══════════════════════════════════════════════════════════════ */
QWidget* MatematicaPage::buildAnalisi2Tab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(10, 8, 10, 8);
    lay->setSpacing(8);

    auto* topRow = new QHBoxLayout;
    auto* topLbl = new QLabel(tr("<b>\xf0\x9f\x93\x99  Argomento:</b>"), w);
    topRow->addWidget(topLbl);

    m_a2TopicCmb = new QComboBox(w);
    m_a2TopicCmb->setObjectName("settingCombo");
    const int nA2 = static_cast<int>(sizeof(kA2)/sizeof(kA2[0]));
    for (int i = 0; i < nA2; ++i)
        m_a2TopicCmb->addItem(QString::fromUtf8(kA2[i].name), i);
    topRow->addWidget(m_a2TopicCmb, 1);

    auto* btnAiExplain = new QPushButton(
        tr("\xf0\x9f\xa4\x96  Spiega con AI"), w);
    btnAiExplain->setObjectName("actionBtn");
    connect(btnAiExplain, &QPushButton::clicked, this, &MatematicaPage::onA2AiClicked);
    topRow->addWidget(btnAiExplain);
    lay->addLayout(topRow);

    /* ─── Splitter orizzontale: Teoria (sx) | Canvas interattivo (dx) ─── */
    auto* hSplit2 = new QSplitter(Qt::Horizontal, w);
    hSplit2->setHandleWidth(5);

    m_a2Theory = new LatexView(hSplit2);
    m_a2Theory->setMinimumWidth(180);
    hSplit2->addWidget(m_a2Theory);

    m_a2Canvas = new GraficoCanvas(hSplit2);
    m_a2Canvas->setMinimumWidth(180);
    hSplit2->addWidget(m_a2Canvas);

    /* ── Pulsante shading viewport stile Blender — overlay nell'angolo del canvas ── */
    {
        struct VM { const char* icon; const char* label; };
        static const VM kVM[] = {
            { "\xe2\x97\x8f", "Punti" },
            { "\xe2\x96\xa1", "Wireframe" },
            { "\xe2\x96\xb3", "Superficie" },
            { "\xe2\x96\xa0", "Solido" },
        };
        auto* vBtn = new QToolButton(m_a2Canvas);
        vBtn->setObjectName("viewportShadingBtn");
        vBtn->setText(QString::fromUtf8(kVM[0].icon));
        vBtn->setToolTip(tr("Shading viewport"));
        vBtn->setAutoRaise(true);
        vBtn->setPopupMode(QToolButton::InstantPopup);
        vBtn->setFixedSize(dpiScale(32), dpiScale(28));
        auto* vMenu = new QMenu(vBtn);
        for (int i = 0; i < 4; ++i) {
            auto* act = vMenu->addAction(
                QString::fromUtf8(kVM[i].icon) + "  " + kVM[i].label);
            act->setData(i);
            connect(act, &QAction::triggered, vBtn,
                    [vBtn, i, this]() {
                        struct VM2 { const char* icon; };
                        static const VM2 kI[] = {{"\xe2\x97\x8f"},{"\xe2\x96\xa1"},{"\xe2\x96\xb3"},{"\xe2\x96\xa0"}};
                        vBtn->setText(QString::fromUtf8(kI[i].icon));
                        onA2RenderChanged(i);
                    });
        }
        vBtn->setMenu(vMenu);
        vBtn->move(dpiScale(6), dpiScale(6));
        vBtn->raise();
    }

    hSplit2->setStretchFactor(0, 3);
    hSplit2->setStretchFactor(1, 4);
    lay->addWidget(hSplit2, 1);

    /* ─── Riga Risolvi ─── */
    auto* tryRow = new QHBoxLayout;
    tryRow->addWidget(new QLabel(tr("\xf0\x9f\x93\x90  Risolvi:"), w));
    m_a2Input = new QLineEdit(w);
    m_a2Input->setPlaceholderText(tr("Espressione SymPy (pre-compilata dall'argomento)"));
    tryRow->addWidget(m_a2Input, 1);
    m_a2TypeCmb = makeAnalisiTypeCmb(w);
    tryRow->addWidget(m_a2TypeCmb);
    auto* btnTry = new QPushButton(
        tr("\xf0\x9f\x93\x90  Risolvi nella scheda accanto"), w);
    btnTry->setObjectName("actionBtn");
    btnTry->setProperty("highlight", "true");
    btnTry->setToolTip(
        "Copia la formula nella scheda \xe2\x80\x9c\xf0\x9f\x93\x90 Risolvi Passi\xe2\x80\x9d "
        "e avvia il calcolo SymPy passo per passo.");
    connect(btnTry, &QPushButton::clicked, this, &MatematicaPage::onA2TryClicked);
    tryRow->addWidget(btnTry);

    auto* btnQuickPlot = new QPushButton(
        tr("\xf0\x9f\x93\x88  Disegna grafico"), w);
    btnQuickPlot->setObjectName("actionBtn");
    btnQuickPlot->setToolTip(tr("Copia l'espressione nel campo Grafico e traccia subito"));
    connect(btnQuickPlot, &QPushButton::clicked, this, [this]() {
        if (m_a2Input && m_a2PlotInput)
            m_a2PlotInput->setText(m_a2Input->text());
        onA2PlotClicked();
    });
    tryRow->addWidget(btnQuickPlot);
    lay->addLayout(tryRow);

    /* ─── Riga Grafico ─── */
    auto* plotRow = new QHBoxLayout;
    plotRow->addWidget(new QLabel(tr("\xf0\x9f\x93\x88  Grafico:"), w));
    m_a2PlotInput = new QLineEdit(w);
    m_a2PlotInput->setPlaceholderText(tr("f(x,y) — con 'y' \xe2\x86\x92 3D"));
    plotRow->addWidget(m_a2PlotInput, 1);
    auto* btnPlot2 = new QPushButton(
        tr("\xf0\x9f\x93\x88  Disegna grafico"), w);
    btnPlot2->setObjectName("actionBtn");
    btnPlot2->setToolTip(tr("Traccia f(x) o f(x,y) nel canvas a destra"));
    connect(btnPlot2, &QPushButton::clicked, this, &MatematicaPage::onA2PlotClicked);
    plotRow->addWidget(btnPlot2);
    /* La barra viewport 3D è un overlay sul canvas (costruita sopra, dopo hSplit2->addWidget) */
    m_btnA2Expand = new QPushButton("\xe2\x86\x97", w);   /* ↗ */
    m_btnA2Expand->setToolTip(tr("Apri grafico in finestra separata"));
    m_btnA2Expand->setFixedWidth(dpiScale(32));
    connect(m_btnA2Expand, &QPushButton::clicked, this, &MatematicaPage::onA2ExpandClicked);
    plotRow->addWidget(m_btnA2Expand);
    lay->addLayout(plotRow);

    connect(m_a2TopicCmb, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MatematicaPage::onA2TopicChanged);
    onA2TopicChanged();

    return w;
}

/* ══════════════════════════════════════════════════════════════
   Slot Analisi 1
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::onA1TopicChanged()
{
    if (!m_a1TopicCmb || !m_a1Theory) return;
    const int idx = m_a1TopicCmb->currentIndex();
    const int nA1 = static_cast<int>(sizeof(kA1)/sizeof(kA1[0]));
    if (idx < 0 || idx >= nA1) return;
    m_a1Theory->setLatexHtml(QString::fromUtf8(kA1[idx].html));
    if (m_a1Input)     m_a1Input->setText(QString::fromUtf8(kA1[idx].ex));
    if (m_a1PlotInput) m_a1PlotInput->setText(QString::fromUtf8(kA1[idx].plotEx));
    if (m_a1TypeCmb) {
        const QString t = QString::fromUtf8(kA1[idx].type);
        const int ti = m_a1TypeCmb->findData(t);
        if (ti >= 0) m_a1TypeCmb->setCurrentIndex(ti);
    }
    if (m_a1Canvas && kA1[idx].plotEx && kA1[idx].plotEx[0]) {
        m_a1Canvas->setCartesian(sympyToCanvas(QString::fromUtf8(kA1[idx].plotEx)), -8.0, 8.0);
        m_a1Canvas->setType(GraficoCanvas::Cartesian);
    }
}

void MatematicaPage::onA1TryClicked()
{
    if (!m_a1Input || !m_a1TypeCmb || !m_solveInput || !m_solveCmb) return;
    const QString expr = m_a1Input->text().trimmed();
    if (expr.isEmpty()) return;
    m_solveInput->setText(expr);
    const QString tipo = m_a1TypeCmb->currentData().toString();
    for (int i = 0; i < m_solveCmb->count(); ++i) {
        if (m_solveCmb->itemText(i) == tipo) {
            m_solveCmb->setCurrentIndex(i); break;
        }
    }

    /* ── Evidenzia zona limite nel canvas Analisi 1 ──────────────────── */
    if (tipo == "Limite" && m_a1Canvas) {
        /* Estrae il punto x→a dall'espressione: formati supportati:
           "f, var, val[, dir]"  oppure  "f as/per var->val" */
        QString xStr;
        static const QRegularExpression reArrow(
            R"((.+?)\s+(?:as|per)\s+\w+\s*->\s*(.+))");
        auto m = reArrow.match(expr);
        if (m.hasMatch()) {
            xStr = m.captured(2).trimmed();
        } else {
            const QStringList parts = expr.split(',');
            if (parts.size() >= 3) xStr = parts[2].trimmed();
        }

        if (!xStr.isEmpty()) {
            bool ok = false;
            const double xVal = xStr.toDouble(&ok);
            if (ok) {
                /* Auto-plot la funzione centrata intorno a x=a */
                const QStringList parts = expr.split(',');
                const QString func = parts.isEmpty() ? QString() : parts[0].trimmed();
                if (!func.isEmpty()) {
                    const double halfW = 6.0;
                    m_a1Canvas->setCartesian(sympyToCanvas(func),
                                             xVal - halfW, xVal + halfW);
                    m_a1Canvas->setType(GraficoCanvas::Cartesian);
                    if (m_a1PlotInput) m_a1PlotInput->setText(func);
                }
                m_a1Canvas->setLimitHighlight(xVal);
                m_limitCanvas = m_a1Canvas;
            }
        }
    } else if (m_limitCanvas) {
        /* Se cambiamo tipo, rimuoviamo il highlight precedente */
        m_limitCanvas->clearLimitHighlight();
        m_limitCanvas = nullptr;
    }

    if (m_solveTabIdx >= 0) m_tabs->setCurrentIndex(m_solveTabIdx);
    onSolveClicked();
}

void MatematicaPage::onA1AiClicked()
{
    if (!m_ai || m_aiRunning) return;
    const int idx = m_a1TopicCmb ? m_a1TopicCmb->currentIndex() : 0;
    const int nA1 = static_cast<int>(sizeof(kA1)/sizeof(kA1[0]));
    if (idx < 0 || idx >= nA1) return;
    const QString topicName = QString::fromUtf8(kA1[idx].name);

    if (m_modelCombo) {
        const QString sel = m_modelCombo->currentData().toString();
        if (!sel.isEmpty() && sel != m_ai->model())
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), sel);
    }
    m_aiRunning = true;
    clearOutput();
    appendOutput(QString("\xf0\x9f\x93\x98  Spiegazione AI: %1\n%2\n\n")
                 .arg(topicName, QString(topicName.length()+20, '-')));
    setStatus(tr("\xf0\x9f\xa4\x96  AI in elaborazione..."));

    const QString sys =
        "Sei un professore universitario di Analisi Matematica 1. "
        "Spiega in italiano il seguente argomento in modo chiaro e didattico: "
        "1) definizione formale con la notazione corretta; "
        "2) teoremi e propriet\xc3\xa0 fondamentali; "
        "3) due esempi svolti passo per passo; "
        "4) un esercizio proposto con soluzione. "
        "Usa OBBLIGATORIAMENTE la notazione LaTeX per le formule: "
        "\\(...\\) per inline, \\[...\\] per display (es. \\(x^2\\), \\[\\int_a^b f\\,dx\\]). "
        "Rispondi SOLO in italiano.";
    const QString user = QString("Argomento: %1").arg(topicName);

    delete m_aiAnalisiHolder;
    m_aiAnalisiHolder = new QObject(this);
    connect(m_ai, &AiClient::token,    m_aiAnalisiHolder,
            [this](const QString& t){ onAnalisiAiToken(t); });
    connect(m_ai, &AiClient::finished, m_aiAnalisiHolder,
            [this](const QString& f){ onAnalisiAiFinished(f); });
    connect(m_ai, &AiClient::error,    m_aiAnalisiHolder,
            [this](const QString& e){ onAnalisiAiError(e); });
    m_ai->chat(P::prependMathKnowledge(sys), user);
}

/* ══════════════════════════════════════════════════════════════
   Slot Analisi 2
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::onA2TopicChanged()
{
    if (!m_a2TopicCmb || !m_a2Theory) return;
    const int idx = m_a2TopicCmb->currentIndex();
    const int nA2 = static_cast<int>(sizeof(kA2)/sizeof(kA2[0]));
    if (idx < 0 || idx >= nA2) return;
    m_a2Theory->setLatexHtml(QString::fromUtf8(kA2[idx].html));
    if (m_a2Input)     m_a2Input->setText(QString::fromUtf8(kA2[idx].ex));
    if (m_a2PlotInput) m_a2PlotInput->setText(QString::fromUtf8(kA2[idx].plotEx));
    if (m_a2TypeCmb) {
        const QString t = QString::fromUtf8(kA2[idx].type);
        const int ti = m_a2TypeCmb->findData(t);
        if (ti >= 0) m_a2TypeCmb->setCurrentIndex(ti);
    }
    if (m_a2Canvas && kA2[idx].plotEx && kA2[idx].plotEx[0]) {
        const QString plotExpr = QString::fromUtf8(kA2[idx].plotEx);
        static const QRegularExpression reY(R"(\by\b)");
        if (plotExpr.contains(reY)) {
            const auto pts = buildSurface3D(plotExpr);
            constexpr int kGridN = 40;
            m_a2Canvas->setScatter3D(pts, kGridN);
            m_a2Canvas->setType(GraficoCanvas::Scatter3D);
        } else {
            m_a2Canvas->setCartesian(sympyToCanvas(plotExpr), -8.0, 8.0);
            m_a2Canvas->setType(GraficoCanvas::Cartesian);
        }
    }
}

void MatematicaPage::onA2TryClicked()
{
    if (!m_a2Input || !m_a2TypeCmb || !m_solveInput || !m_solveCmb) return;
    const QString expr = m_a2Input->text().trimmed();
    if (expr.isEmpty()) return;
    m_solveInput->setText(expr);
    const QString tipo = m_a2TypeCmb->currentData().toString();
    for (int i = 0; i < m_solveCmb->count(); ++i) {
        if (m_solveCmb->itemText(i) == tipo) {
            m_solveCmb->setCurrentIndex(i); break;
        }
    }
    if (m_solveTabIdx >= 0) m_tabs->setCurrentIndex(m_solveTabIdx);
    onSolveClicked();
}

void MatematicaPage::onA2AiClicked()
{
    if (!m_ai || m_aiRunning) return;
    const int idx = m_a2TopicCmb ? m_a2TopicCmb->currentIndex() : 0;
    const int nA2 = static_cast<int>(sizeof(kA2)/sizeof(kA2[0]));
    if (idx < 0 || idx >= nA2) return;
    const QString topicName = QString::fromUtf8(kA2[idx].name);

    if (m_modelCombo) {
        const QString sel = m_modelCombo->currentData().toString();
        if (!sel.isEmpty() && sel != m_ai->model())
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), sel);
    }
    m_aiRunning = true;
    clearOutput();
    appendOutput(QString("\xf0\x9f\x93\x99  Spiegazione AI: %1\n%2\n\n")
                 .arg(topicName, QString(topicName.length()+20, '-')));
    setStatus(tr("\xf0\x9f\xa4\x96  AI in elaborazione..."));

    const QString sys =
        "Sei un professore universitario di Analisi Matematica 2. "
        "Spiega in italiano il seguente argomento in modo chiaro e didattico: "
        "1) definizione formale; 2) teoremi chiave; "
        "3) due esempi svolti passo per passo; "
        "4) un esercizio proposto con soluzione. "
        "Usa OBBLIGATORIAMENTE la notazione LaTeX per le formule: "
        "\\(...\\) per inline, \\[...\\] per display. "
        "Usa notazione vettoriale italiana. Rispondi SOLO in italiano.";
    const QString user = QString("Argomento: %1").arg(topicName);

    delete m_aiAnalisiHolder;
    m_aiAnalisiHolder = new QObject(this);
    connect(m_ai, &AiClient::token,    m_aiAnalisiHolder,
            [this](const QString& t){ onAnalisiAiToken(t); });
    connect(m_ai, &AiClient::finished, m_aiAnalisiHolder,
            [this](const QString& f){ onAnalisiAiFinished(f); });
    connect(m_ai, &AiClient::error,    m_aiAnalisiHolder,
            [this](const QString& e){ onAnalisiAiError(e); });
    m_ai->chat(P::prependMathKnowledge(sys), user);
}

/* ══════════════════════════════════════════════════════════════
   Slot AI Analisi (one-shot shared)
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::onAnalisiAiToken(const QString& tok)
{
    appendOutput(tok);
}

void MatematicaPage::onAnalisiAiFinished(const QString& full)
{
    delete m_aiAnalisiHolder;
    m_aiAnalisiHolder = nullptr;
    m_aiRunning = false;
    setStatus(tr("\xe2\x9c\x85  Spiegazione AI completata."));
    if (m_latexOut && m_latexOut->isVisible() && !full.isEmpty()) {
        /* toHtmlEscaped() non tocca \ — i delimitatori \(...\) e \[...\] rimangono
           intatti; KaTeX auto-render li trova e renderizza le formule. */
        m_latexOut->setLatexHtml(
            "<div class='ai-out'>" + full.toHtmlEscaped().replace("\n", "<br>") + "</div>");
    }
}

void MatematicaPage::onAnalisiAiError(const QString& msg)
{
    delete m_aiAnalisiHolder;
    m_aiAnalisiHolder = nullptr;
    m_aiRunning = false;
    appendOutput("\n\xe2\x9d\x8c  Errore AI: " + msg + "\n");
    setStatus(tr("\xe2\x9d\x8c  Errore AI."));
    LogBus::post("\xe2\x9d\x8c Matematica: Errore AI analisi: " + msg);
}

/* ══════════════════════════════════════════════════════════════
   Grafici Analisi — GraficoCanvas nativo (zero subprocess)
   ══════════════════════════════════════════════════════════════ */

/* Converte notazione SymPy → FormulaParser:
   **  → ^
   log → ln  (SymPy usa log per log naturale; il parser usa ln)
   Exp → exp (case insensitive non necessario, SymPy emette minuscolo) */
QString MatematicaPage::sympyToCanvas(const QString& expr)
{
    QString r = expr;
    r.replace("**", "^");
    /* log(x) SymPy = logaritmo naturale → ln(x) per il parser */
    static const QRegularExpression reLog(R"(\blog\s*\()");
    r.replace(reLog, "ln(");
    return r;
}

/* Genera una griglia 3D da f(x,y): rimpiazza \by\b con il valore numerico
   e usa FormulaParser (già in uso nel canvas) per valutare f_y(x). */
QVector<GraficoCanvas::Pt3D> MatematicaPage::buildSurface3D(const QString& exprSym)
{
    const QString expr = sympyToCanvas(exprSym);
    constexpr int N = 40;
    constexpr double R = 4.0;
    constexpr double step = 2.0 * R / (N - 1);
    static const QRegularExpression reY(R"(\by\b)");

    /* Griglia sempre completa N×N — punti invalidi ricevono z=NaN.
       Questo preserva la topologia richiesta per Wireframe e Surface. */
    const double kNaN = std::numeric_limits<double>::quiet_NaN();
    QVector<GraficoCanvas::Pt3D> pts(N * N);
    for (int iy = 0; iy < N; ++iy) {
        const double yVal = -R + iy * step;
        QString formulaY = expr;
        formulaY.replace(reY, QString::number(yVal, 'g', 8));
        FormulaParser fp(formulaY);
        for (int ix = 0; ix < N; ++ix) {
            const double xVal = -R + ix * step;
            double z = kNaN;
            if (fp.ok()) {
                const double v = fp.eval(xVal);
                if (std::isfinite(v) && std::abs(v) < 1e5)
                    z = v;
            }
            pts[iy * N + ix] = {xVal, yVal, z, {}};
        }
    }
    return pts;
}

void MatematicaPage::onA1PlotClicked()
{
    if (!m_a1Canvas || !m_a1PlotInput) return;
    const QString expr = m_a1PlotInput->text().trimmed();
    if (expr.isEmpty()) return;
    const int mode = m_a1RenderCmb ? m_a1RenderCmb->currentIndex() : 0;
    if (mode == 1) {
        FormulaParser fp(sympyToCanvas(expr));
        m_a1Canvas->setScatter(fp.sample(-8.0, 8.0, 300));
        m_a1Canvas->setType(GraficoCanvas::ScatterXY);
    } else if (mode == 2) {
        FormulaParser fp(sympyToCanvas(expr));
        m_a1Canvas->setLine({fp.sample(-8.0, 8.0, 300)}, {});
        m_a1Canvas->setType(GraficoCanvas::Area);
    } else {
        m_a1Canvas->setCartesian(sympyToCanvas(expr), -8.0, 8.0);
        m_a1Canvas->setType(GraficoCanvas::Cartesian);
    }
}

void MatematicaPage::onA1RenderChanged(int /*idx*/)
{
    if (!m_a1PlotInput || m_a1PlotInput->text().trimmed().isEmpty()) return;
    onA1PlotClicked();
}

void MatematicaPage::onA2PlotClicked()
{
    if (!m_a2Canvas || !m_a2PlotInput) return;
    const QString expr = m_a2PlotInput->text().trimmed();
    if (expr.isEmpty()) return;
    static const QRegularExpression reY(R"(\by\b)");
    if (expr.contains(reY)) {
        constexpr int kGridN = 40;
        const auto pts = buildSurface3D(expr);
        m_a2Canvas->setScatter3D(pts, kGridN);   /* passa topologia griglia */
        m_a2Canvas->setType(GraficoCanvas::Scatter3D);
    } else {
        m_a2Canvas->setCartesian(sympyToCanvas(expr), -8.0, 8.0);
        m_a2Canvas->setType(GraficoCanvas::Cartesian);
    }
}

void MatematicaPage::onA2RenderChanged(int idx)
{
    if (!m_a2Canvas) return;
    m_a2Canvas->setRenderMode3D(static_cast<GraficoCanvas::RenderMode3D>(idx));
}

/* helper — apre il canvas sorgente in un QDialog non-modale */
static void openCanvasInWindow(GraficoCanvas* src, const QString& title, QWidget* parent)
{
    if (!src) return;
    auto* dlg = new QDialog(parent, Qt::Window);
    dlg->setWindowTitle(title);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(800, 600);
    auto* lay = new QVBoxLayout(dlg);
    lay->setContentsMargins(4, 4, 4, 4);
    auto* canvas = new GraficoCanvas(dlg);

    /* copia stato */
    switch (src->currentType()) {
    case GraficoCanvas::Scatter3D:
        canvas->setScatter3D(src->pts3d(), src->grid3dCols());
        canvas->setType(GraficoCanvas::Scatter3D);
        canvas->setRenderMode3D(src->renderMode3D());
        break;
    case GraficoCanvas::ScatterXY:
        canvas->setScatter(src->scatterPts());
        canvas->setType(GraficoCanvas::ScatterXY);
        break;
    case GraficoCanvas::Area:
        canvas->setLine(src->lineSeries(), {});
        canvas->setType(GraficoCanvas::Area);
        break;
    default:
        canvas->setCartesian(src->cartFormula(), src->cartXMin(), src->cartXMax());
        canvas->setType(GraficoCanvas::Cartesian);
        break;
    }
    lay->addWidget(canvas);
    dlg->show();
}

void MatematicaPage::onA1ExpandClicked()
{
    openCanvasInWindow(m_a1Canvas, "Analisi 1 — Grafico", this);
}

void MatematicaPage::onA2ExpandClicked()
{
    openCanvasInWindow(m_a2Canvas, "Analisi 2 — Grafico 3D", this);
}

/* ══════════════════════════════════════════════════════════════
   buildBoolTab — Algebra Booleana: operatori, De Morgan, teoremi
   ══════════════════════════════════════════════════════════════ */
