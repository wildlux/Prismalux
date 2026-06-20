/* main_math_solve.cpp — Risolvi Passi: UI + slot */
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

static int mathModelScore(const QString& name)
{
    const QString n = name.toLower();
    if (n.contains("qwen2.5-math") || n.contains("qwen2_5-math")) return 100;
    if (n.contains("mathstral") || n.contains("math"))             return 90;
    if (n.contains("deepseek-r1"))  return 85;
    if (n.contains("qwq"))          return 80;
    if (n.contains("phi-4") || n.contains("phi4")) return 75;
    if (n.contains("deepseek") && !n.contains("coder")) return 60;
    if (n.contains("qwen")     && !n.contains("coder")) return 50;
    return 0;
}

 /*
   buildSympyScript — genera lo script Python/SymPy per ogni tipo
   ══════════════════════════════════════════════════════════════ */
static QString buildSympyScript(const QString& tipo, const QString& expr)
{
    /* Escape dell'espressione utente per inniezione sicura nel Python */
    QString safe = expr;
    safe.replace("\\", "\\\\").replace("'", "\\'").replace("\n", " ").replace("\r", "");

    /* Preambolo comune */
    QString py = QString(R"SCRIPT(
import sys, re, platform
import sympy as _sympy_mod
from sympy import *
import re  # ripristina: from sympy import * sovrascrive re con sympy.re()
from sympy.parsing.sympy_parser import (
    parse_expr, standard_transformations,
    implicit_multiplication_application, convert_xor)
x,y,z,n,t,a,b,c,k = symbols('x y z n t a b c k')
TR = standard_transformations + (implicit_multiplication_application, convert_xor)
LD = {'x':x,'y':y,'z':z,'n':n,'t':t,'a':a,'b':b,'c':c,'k':k,
      'pi':pi,'e':E,'I':I,'oo':oo,'inf':oo,
      'sqrt':sqrt,'sin':sin,'cos':cos,'tan':tan,
      'asin':asin,'acos':acos,'atan':atan,
      'log':log,'ln':log,'exp':exp,'abs':Abs}
LINE = '─'*56
def parse(s):
    return parse_expr(s.strip(), transformations=TR, local_dict=LD)
def pprint_inline(expr):
    """Restituisce una stringa 'da foglio': usa pretty() su una riga se possibile."""
    try:
        p = str(pretty(expr, use_unicode=True))
        # se pretty è multiriga prende solo la prima riga di una repr compatta
        lines = [l for l in p.splitlines() if l.strip()]
        if len(lines) == 1:
            return lines[0].strip()
    except:
        pass
    return str(expr)
def fmt(v):
    try:
        ev = complex(v.evalf(8))
        if abs(ev.imag) < 1e-7:
            f = ev.real
            base = pprint_inline(v)
            return base if abs(f - round(f)) < 1e-9 else f'{base}  ≈  {f:.6g}'
        else:
            return f'{pprint_inline(v)}  ≈  {ev.real:.5g} {ev.imag:+.5g}i'
    except:
        return pprint_inline(v)
print(LINE)
print('  Risolvi Passi — SymPy Engine')
print(f'  Python {platform.python_version()}  |  SymPy {_sympy_mod.__version__}')
print('  Motore: calcolo simbolico esatto (nessun LLM)')
print(LINE)
print()
)SCRIPT");

    py += QString("expr_str = '%1'\n").arg(safe);
    py += "print(f'Problema: {expr_str}')\nprint()\n";
    py += "try:\n";

    if (tipo == "Equazione") {
        py += R"SCRIPT(
    if '=' in expr_str:
        parts = expr_str.split('=', 1)
        lhs = parse(parts[0]); rhs = parse(parts[1])
    else:
        lhs = parse(expr_str); rhs = S.Zero
    residual = lhs - rhs
    free = residual.free_symbols
    var = x if x in free else (next(iter(free)) if free else x)

    print(f'PASSO 1 — Riscrittura in f({var})=0')
    print(f'  {lhs} = {rhs}  →  {residual} = 0')
    print(f'  Variabile: {var}')
    print(f'  ↳ Perché: portare tutto a sinistra crea la forma canonica f({var})=0,')
    print(f'    necessaria per applicare i teoremi di esistenza delle radici e gli')
    print(f'    algoritmi di risoluzione simbolica.\n')

    exp2 = expand(residual)
    if str(exp2) != str(residual):
        print(f'PASSO 2 — Espansione')
        print(f'  {exp2} = 0')
        print(f'  ↳ Perché: distribuire prodotti e potenze rivela i termini simili,')
        print(f'    rende visibile il grado e prepara alla fattorizzazione.\n')

    fac = factor(residual)
    if str(fac) not in (str(residual), str(exp2)):
        print(f'PASSO 3 — Fattorizzazione')
        print(f'  {fac} = 0')
        print(f'  ↳ Perché: scrivere f come prodotto di fattori lineari/irriducibili')
        print(f'    permette di leggere le radici direttamente (ogni fattore = 0).\n')

    if residual.is_polynomial(var):
        p = Poly(residual, var)
        deg = p.degree()
        print(f'PASSO 4 — Analisi polinomiale')
        print(f'  Grado: {deg}   Coefficienti [a_n…a_0]: {p.all_coeffs()}')
        print(f'  ↳ Perché: il grado stabilisce il numero massimo di soluzioni')
        print(f'    (Teorema Fondamentale dell\'Algebra: n radici contando molteplicità).')
        if deg == 2:
            cf = p.all_coeffs()
            a2 = Rational(cf[0]); b2 = Rational(cf[1]); c2 = Rational(cf[2])
            disc = b2**2 - 4*a2*c2
            print(f'  Discriminante Δ = ({b2})² - 4·({a2})·({c2}) = {disc}')
            if disc > 0:
                print(f'  ↳ Δ>0: due radici reali distinte (formula quadratica).')
            elif disc == 0:
                print(f'  ↳ Δ=0: radice doppia (tangente all\'asse x).')
            else:
                print(f'  ↳ Δ<0: nessuna radice reale (le soluzioni sono complesse coniugate).')
        print()

    print('PASSO 5 — Calcolo soluzioni')
    print('  ↳ Perché: SymPy applica formule chiuse (quadratica, cubica, quartica)')
    print('    se esistono; altrimenti usa algoritmi numerici (Newton-Raphson).')
    sols = solve(Eq(lhs, rhs), var) or solve(residual, var)
    if sols:
        print(f'  Soluzioni trovate: {len(sols)}')
        for i,s in enumerate(sols,1):
            print(f'    {var}_{i} = {fmt(s)}')
    else:
        print('  Nessuna soluzione simbolica. Ricerca numerica:')
        found = set()
        for x0 in [0,1,-1,2,-2,5,-5,10,-10,0.5,-0.5]:
            try:
                s = nsolve(residual, var, x0, tol=1e-8, verify=False)
                sv = round(float(s), 6)
                if all(abs(sv-f) > 1e-4 for f in found):
                    found.add(sv); print(f'    Radice ≈ {sv}  (vicino a {x0})')
            except: pass
        if not found: print('    Nessuna radice reale trovata.')
    print()
    print(LINE); print('SOLUZIONE FINALE:')
    for i,s in enumerate(sols or [],1): print(f'  {var}_{i} = {s}')
    if not sols: print('  (vedi ricerca numerica sopra)')
    print(LINE)
)SCRIPT";

    } else if (tipo == "Disequazione") {
        py += R"SCRIPT(
    op_m = re.search(r'(<=|>=|<|>)', expr_str)
    if not op_m:
        print('Errore: inserisci un operatore < > <= >=', file=sys.stderr); sys.exit(1)
    op = op_m.group(1)
    parts = re.split(r'<=|>=|<|>', expr_str, 1)
    lhs = parse(parts[0]); rhs = parse(parts[1])
    residual = lhs - rhs
    free = residual.free_symbols
    var = x if x in free else (next(iter(free)) if free else x)

    print(f'PASSO 1 — Riscrittura in forma standard')
    print(f'  {lhs} {op} {rhs}  →  {residual} {op} 0')
    print(f'  ↳ Perché: portare tutto a sinistra riduce il problema allo studio')
    print(f'    del segno di una sola espressione, tecnica fondamentale per')
    print(f'    le disequazioni algebriche e trascendenti.\n')

    fac = factor(residual)
    if str(fac) != str(residual):
        print(f'PASSO 2 — Fattorizzazione')
        print(f'  {fac} {op} 0')
        print(f'  ↳ Perché: un prodotto di fattori cambia segno solo negli zeri di')
        print(f'    ciascun fattore; la tabella dei segni si costruisce fattore per')
        print(f'    fattore (metodo degli intervalli).\n')

    print('PASSO 3 — Studio del segno e insieme soluzione')
    zeros = solve(residual, var)
    if zeros:
        print(f'  Zeri di f({var}): {zeros}')
        print(f'  ↳ Gli zeri sono i punti di confine degli intervalli di soluzione.')
    from sympy.solvers.inequalities import solve_univariate_inequality
    op_map = {'<': lhs < rhs, '>': lhs > rhs, '<=': lhs <= rhs, '>=': lhs >= rhs}
    result = solve_univariate_inequality(op_map[op], var, relational=False)
    print(f'  Insieme soluzione: {result}')
    print(f'  ↳ SymPy unisce gli intervalli dove f({var}) ha il segno richiesto')
    print(f'    (positivo per ">", negativo per "<", incluso gli zeri per "≥","≤").\n')
    print(LINE); print(f'SOLUZIONE FINALE:\n  {var} ∈ {result}'); print(LINE)
)SCRIPT";

    } else if (tipo == "Derivata") {
        py += R"SCRIPT(
    parts = [p.strip() for p in expr_str.split(',')]
    f_str = parts[0]
    var_str = parts[1].strip() if len(parts) > 1 else 'x'
    order = int(parts[2].strip()) if len(parts) > 2 else 1
    f = parse(f_str)
    var = parse(var_str)

    print(f'PASSO 1 — Riconoscimento della struttura')
    print(f'  f({var}) = {f}')
    print(f'  ↳ Perché: identificare se la funzione è una somma, prodotto,')
    print(f'    quoziente o composizione determina quale regola di derivazione')
    print(f'    applicare (linearità, Leibniz, regola della catena).\n')

    fs = simplify(f)
    if str(fs) != str(f):
        print(f'PASSO 2 — Pre-semplificazione')
        print(f'  f({var}) = {fs}')
        print(f'  ↳ Perché: una forma più semplice riduce il numero di termini')
        print(f'    su cui applicare le regole e limita gli errori algebrici.\n')

    df = diff(f, var, order)
    label = f"{''.join([\"'\"]*order)}"
    ord_name = 'prima' if order==1 else (str(order)+'ª')
    print(f'PASSO 3 — Derivata {ord_name} rispetto a {var}')
    print(f'  f{label}({var}) = {df}')
    rules = []
    if f.is_Add: rules.append('regola della somma (linearità)')
    if f.is_Mul: rules.append('regola del prodotto (Leibniz)')
    if f.is_Pow: rules.append('regola della potenza')
    if f.has(sin,cos,tan,exp,log): rules.append('derivata di funzioni elementari')
    rule_str = ', '.join(rules) if rules else 'regole standard di derivazione'
    print(f'  ↳ Applica: {rule_str}.')
    print(f'    La derivata misura il tasso di variazione istantaneo di f in {var}.\n')

    dfs = simplify(df)
    if str(dfs) != str(df):
        print(f'PASSO 4 — Semplificazione della derivata')
        print(f'  f{label}({var}) = {dfs}')
        print(f'  ↳ Perché: la forma semplificata è più leggibile e più utile')
        print(f'    per trovare zeri, segno e comportamento asintotico.\n')
        df = dfs

    if order == 1:
        cps = solve(df, var)
        if cps:
            print(f'PASSO 5 — Punti critici (f\'({var})=0)')
            print(f'  ↳ Perché: dove la derivata si annulla la funzione ha')
            print(f'    tangente orizzontale → candidati a massimo, minimo o flesso.')
            for i,cp in enumerate(cps,1):
                try: yv = f.subs(var,cp)
                except: yv = '?'
                d2 = diff(f, var, 2).subs(var, cp) if cps else None
                nature = ''
                try:
                    if d2 is not None:
                        if d2 > 0: nature = ' → minimo locale (f\'\'> 0)'
                        elif d2 < 0: nature = ' → massimo locale (f\'\'< 0)'
                        else: nature = ' → analisi ordine superiore necessaria'
                except: pass
                print(f'  {var}_{i} = {fmt(cp)},  f = {yv}{nature}')
            print()

    print(LINE); print(f'SOLUZIONE FINALE:\n  f{label}({var}) = {df}'); print(LINE)
)SCRIPT";

    } else if (tipo == "Integrale") {
        py += R"SCRIPT(
    parts = [p.strip() for p in expr_str.split(',')]
    f = parse(parts[0])
    free = f.free_symbols
    if len(parts) == 1:
        var = x if x in free else (next(iter(free)) if free else x)
        a_val = b_val = None
    elif len(parts) == 2:
        var = parse(parts[1]); a_val = b_val = None
    else:
        var = x if x in free else (next(iter(free)) if free else x)
        a_val = parse(parts[1]); b_val = parse(parts[2])

    if a_val is None:
        print(f'PASSO 1 — Identificazione: integrale indefinito')
        print(f'  ∫ {f} d{var}')
        print(f'  ↳ Perché: cerchiamo una funzione F tale che F\'({var})=f({var}).')
        print(f'    La costante +C riflette il fatto che infinite primitive differiscono')
        print(f'    per una costante additiva (famiglie di curve parallele).\n')
        result = integrate(f, var)
        rs = simplify(result)
        # Identifica la tecnica usata
        tech = 'integrazione diretta (tabelle standard)'
        if f.has(exp): tech = 'integrale di funzione esponenziale'
        elif f.has(log): tech = 'integrazione per parti (possibile)'
        elif f.is_polynomial(var): tech = 'regola della potenza ∫xⁿdx = xⁿ⁺¹/(n+1)'
        elif f.has(sin) or f.has(cos): tech = 'integrale di funzione trigonometrica'
        print(f'PASSO 2 — Calcolo della primitiva')
        print(f'  F({var}) = {rs}')
        print(f'  ↳ Tecnica: {tech}.')
        print(f'    SymPy usa heuristic integration, Risch algorithm o integrazione')
        print(f'    per parti/sostituzione a seconda della struttura dell\'integrando.\n')
        print(LINE); print(f'SOLUZIONE FINALE:\n  ∫ {f} d{var} = {rs} + C'); print(LINE)
    else:
        print(f'PASSO 1 — Identificazione: integrale definito')
        print(f'  ∫[{a_val}…{b_val}] {f} d{var}')
        print(f'  ↳ Perché: l\'integrale definito misura l\'area algebrica (con segno)')
        print(f'    sotto la curva f({var}) tra {a_val} e {b_val}.\n')
        indef = integrate(f, var)
        rs = simplify(indef)
        print(f'PASSO 2 — Primitiva F({var})')
        print(f'  F({var}) = {rs}')
        print(f'  ↳ Perché: trovare la primitiva è il passo obbligatorio prima di')
        print(f'    applicare il Teorema Fondamentale del Calcolo Integrale.\n')
        fa = rs.subs(var, b_val); fb = rs.subs(var, a_val)
        result = simplify(fa - fb)
        print(f'PASSO 3 — Teorema Fondamentale del Calcolo (Newton-Leibniz)')
        print(f'  F({b_val}) − F({a_val}) = ({fa}) − ({fb})')
        print(f'  = {result}')
        print(f'  ↳ Perché: il Teorema Fondamentale afferma ∫[a,b]f = F(b)−F(a),')
        print(f'    collegando integrazione e derivazione (operazioni inverse).')
        print(f'    Questo evita di calcolare la somma di Riemann con infiniti rettangoli.\n')
        print(LINE); print(f'SOLUZIONE FINALE:\n  ∫ = {result}  ({fmt(result)})'); print(LINE)
)SCRIPT";

    } else if (tipo == "Limite") {
        py += R"SCRIPT(
    # formati: "f, var, val[, dir]" oppure "f as var->val" oppure "f per var->val"
    m = re.match(r'(.+?)\s+(?:as|per|when)\s+(\w+)\s*->\s*(.+)', expr_str)
    if m:
        f_str, var_str, val_str, dir_str = m.group(1), m.group(2), m.group(3), '+-'
    else:
        parts = [p.strip() for p in expr_str.split(',')]
        f_str = parts[0]
        free = parse(f_str).free_symbols
        var_str = parts[1] if len(parts)>1 else (str(x) if x in free else (str(next(iter(free))) if free else 'x'))
        val_str = parts[2] if len(parts)>2 else '0'
        dir_str = parts[3] if len(parts)>3 else '+-'

    f = parse(f_str); var = parse(var_str); val = parse(val_str)
    dir_str = dir_str.strip()
    dir_label = '' if dir_str == '+-' else f' ({dir_str})'

    print(f'PASSO 1 — Analisi del problema')
    print(f'  lim[{var}→{val}{dir_label}]  {f}')
    print(f'  ↳ Perché: il limite descrive il comportamento di f({var}) quando {var} si')
    print(f'    avvicina a {val}, anche se f non è definita o continua in quel punto.')
    if dir_str in ('+','-'):
        side = 'destra' if dir_str=='+' else 'sinistra'
        print(f'    Il limite laterale ({side}) si usa quando la funzione ha un')
        print(f'    comportamento diverso a seconda da che lato si approccia {val}.')
    print()

    determined = False
    try:
        direct = f.subs(var, val)
        # Solo confronti 'is' — .has() e 'not in (...)' innescano
        # il bug SymPy 1.14 / Python 3.14: 'tuple has no attribute matches'
        _bad = (direct is zoo or direct is nan or
                direct is oo  or direct is -oo)
        if not _bad:
            # verifica ulteriore: deve essere un valore finito valutabile
            try:
                ev = float(direct.evalf(6))
                if -1e15 < ev < 1e15:
                    print(f'PASSO 2 — Sostituzione diretta (continuità)')
                    print(f'  f({val}) = {fmt(direct)}')
                    print(f'  ↳ Perché: se f è continua in {val}, il limite coincide con il valore')
                    print(f'    della funzione. La sostituzione diretta è il metodo più rapido.')
                    print(f'    Se avesse dato 0/0, ∞/∞ ecc. sarebbe stata necessaria l\'analisi')
                    print(f'    delle forme indeterminate (L\'Hôpital, sviluppi di Taylor).\n')
                    determined = True
            except: pass
    except: pass

    print(f'PASSO 3 — Calcolo limite simbolico')
    if not determined:
        print(f'  ↳ Perché: la sostituzione diretta ha generato una forma indeterminata.')
        print(f'    SymPy usa algebricamente: semplificazione razionale, sviluppi')
        print(f'    di Taylor/McLaurin, regola di L\'Hôpital o sostituzione trigonometrica.')
    if dir_str in ('+', '-'):
        result = limit(f, var, val, dir_str)
    else:
        result = limit(f, var, val)
    print(f'  lim[{var}→{val}{dir_label}] {f} = {result}')
    try:
        _inf = result is oo or result is -oo or result is zoo or result is nan
        if not _inf:
            print(f'  Valore numerico: {fmt(result)}')
        elif result is oo:
            print(f'  ↳ Il limite è +∞: la funzione diverge (cresce senza limite).')
        elif result is -oo:
            print(f'  ↳ Il limite è -∞: la funzione diverge negativamente.')
        elif result is zoo:
            print(f'  ↳ Il limite è ∞ complesso: la funzione oscilla o diverge.')
        elif result is nan:
            print(f'  ↳ Forma indeterminata irrisolvibile simbolicamente.')
    except: pass
    print()
    print(LINE); print(f'SOLUZIONE FINALE:\n  lim[{var}→{val}{dir_label}] {f} = {result}'); print(LINE)
)SCRIPT";

    } else { /* Semplificazione */
        py += R"SCRIPT(
    f = parse(expr_str)
    print(f'PASSO 1 — Forma originale di partenza')
    print(f'  {f}')
    print(f'  ↳ Perché: registrare la forma iniziale permette di confrontare')
    print(f'    le trasformazioni successive e verificare l\'equivalenza algebrica.\n')
    results = []

    exp2 = expand(f)
    if str(exp2) != str(f):
        print(f'PASSO 2 — Espansione')
        print(f'  {exp2}')
        print(f'  ↳ Perché: distribuire prodotti e binomi rivela i termini simili,')
        print(f'    utile per semplificare frazioni algebriche e trovare i coefficienti.\n')
        results.append(exp2)

    fac = factor(f)
    if str(fac) not in (str(f), str(exp2)):
        print(f'PASSO 3 — Fattorizzazione')
        print(f'  {fac}')
        print(f'  ↳ Perché: la forma fattorizzata evidenzia le radici e semplifica')
        print(f'    frazioni (cancellando fattori comuni al numeratore/denominatore).\n')
        results.append(fac)

    simp = simplify(f)
    if str(simp) not in [str(r) for r in [f,exp2,fac]]:
        print(f'PASSO 4 — Simplificazione generale (simplify)')
        print(f'  {simp}')
        print(f'  ↳ Perché: simplify() prova internamente expand, factor, trigsimp,')
        print(f'    radsimp e altre strategie, scegliendo la forma con meno operazioni.\n')
        results.append(simp)

    ts = trigsimp(f)
    if str(ts) not in [str(r) for r in [f,exp2,fac,simp]]:
        print(f'PASSO 5 — Semplificazione trigonometrica (trigsimp)')
        print(f'  {ts}')
        print(f'  ↳ Perché: usa identità fondamentali (sin²+cos²=1, formule di')
        print(f'    addizione, ecc.) per ridurre espressioni trigonometriche.\n')
        results.append(ts)

    ps = powsimp(f, deep=True)
    if str(ps) not in [str(r) for r in [f,exp2,fac,simp,ts]]:
        print(f'PASSO 6 — Semplificazione di potenze (powsimp)')
        print(f'  {ps}')
        print(f'  ↳ Perché: raccoglie xᵃ·xᵇ = xᵃ⁺ᵇ e (xᵃ)ᵇ = xᵃᵇ, riducendo')
        print(f'    il numero di operazioni aritmetiche nella forma finale.\n')
        results.append(ps)

    best = min([f]+results, key=lambda v: len(str(v)))
    print(LINE); print(f'SOLUZIONE FINALE (forma più compatta):\n  {best}'); print(LINE)
)SCRIPT";
    }

    py += R"SCRIPT(
except Exception as e:
    print(f'Errore SymPy: {e}', file=sys.stderr)
    sys.exit(1)
)SCRIPT";

    return py;
}


/* ── Genera avviso dominio + suggerisci modello migliore disponibile ─
   Restituisce un messaggio HTML da mostrare nell'output.
   Se il modello corrente è già math-capable restituisce stringa vuota. */
static QString buildDomainHint(AiClient* ai, const QString& query,
                               const QComboBox* combo)
{
    using D = AiClient::QueryDomain;
    const D dom = AiClient::detectQueryDomain(query);
    if (!AiClient::domainNeedsMathModel(dom)) return {};

    const QString cur = ai ? ai->model() : QString();
    if (mathModelScore(cur) >= 50) return {};  /* già adatto */

    /* Cerca il migliore disponibile nella combo */
    QString best;
    int bestSc = -1;
    if (combo) {
        for (int i = 0; i < combo->count(); ++i) {
            const QString m = combo->itemData(i).toString();
            const int sc = mathModelScore(m);
            if (sc > bestSc) { bestSc = sc; best = m; }
        }
    }

    static const char* domLabel[] = {
        nullptr, "matematica", "fisica", "chimica", "elettronica", nullptr, nullptr
    };
    const char* dl = (dom < 7) ? domLabel[dom] : nullptr;
    QString msg = QString(
        "\xf0\x9f\x92\xa1  <b>Query di %1</b> rilevata. "
        "Il modello attuale (<code>%2</code>) non \xc3\xa8 specializzato per calcoli STEM.")
        .arg(dl ? dl : "scienze").arg(cur.isEmpty() ? "nessuno" : cur);

    if (!best.isEmpty() && bestSc >= 50)
        msg += QString(" <span style='color:#a3e635;'>Consigliato: <b>%1</b> "
                       "(punteggio math: %2)</span>.").arg(best).arg(bestSc);
    else
        msg += " Installa <code>qwen2.5-math:7b</code> o <code>deepseek-r1:7b</code> "
               "per risultati migliori.";

    return msg;
}


struct SolveExample { const char* expr; const char* tipo; const char* desc; };
static const SolveExample kSolveExamples[] = {
    /* ── Equazioni (10) ──────────────────────────────────── */
    { "x^3 - 6*x^2 + 11*x - 6 = 0",
      "Equazione", "Terzo grado - radici razionali 1, 2, 3 [TEST]" },
    { "x^4 - 13*x^2 + 36 = 0",
      "Equazione", "Biquadratica - radici +-2, +-3 [TEST]" },
    { "x^2 + x + 1 = 0",
      "Equazione", "Delta < 0 - radici complesse coniugate [TEST]" },
    { "2*x^3 + x^2 - 5*x + 2 = 0",
      "Equazione", "Cubica con radici 1/2, 1, -2" },
    { "x^4 - 5*x^2 + 4 = 0",
      "Equazione", "Biquadratica - radici +-1, +-2" },
    { "x^3 + 3*x^2 - 4 = 0",
      "Equazione", "Cubica con fattore (x-1)" },
    { "x^2 - 2*sqrt(3)*x + 3 = 0",
      "Equazione", "Delta = 0 - radice doppia sqrt(3)" },
    { "x^5 - x = 0",
      "Equazione", "Quintico fattorizzabile - 5 radici" },
    { "x^2 - 5 = 0",
      "Equazione", "Radici irrazionali +-sqrt(5)" },
    { "x^3 - x = 0",
      "Equazione", "Cubica - radici 0, +-1" },
    /* ── Disequazioni (5) ────────────────────────────────── */
    { "x^2 - 5*x + 6 > 0",
      "Disequazione", "Parabola > 0 su (-oo,2) U (3,+oo) [TEST]" },
    { "x^3 - x > 0",
      "Disequazione", "Cubica positiva in (-1,0) U (1,+oo)" },
    { "(x - 1)*(x + 2)*(x - 3) < 0",
      "Disequazione", "Tre radici 1, -2, 3" },
    { "x^4 - 5*x^2 + 4 <= 0",
      "Disequazione", "Biquadratica <= 0 su [-2,-1] U [1,2]" },
    { "x^2 - 4 >= 0",
      "Disequazione", "Parabola >= 0 su (-oo,-2] U [2,+oo)" },
    /* ── Derivate (10) ───────────────────────────────────── */
    { "sin(x^2 + 1), x",
      "Derivata", "Catena: D[sin(x^2+1)] = 2x*cos(x^2+1) [TEST]" },
    { "x^3*exp(x), x, 2",
      "Derivata", "Derivata seconda di x^3*e^x [TEST]" },
    { "atan(x), x",
      "Derivata", "D[arctan(x)] = 1/(1+x^2) [TEST]" },
    { "log(x^2 + 1)*sin(x), x",
      "Derivata", "Prodotto ln(x^2+1)*sin(x)" },
    { "(x^2 + 1)/(x^3 - 1), x",
      "Derivata", "Derivata di funzione razionale" },
    { "sin(x)^2, x",
      "Derivata", "D[sin^2(x)] = sin(2x)" },
    { "sqrt(x^2 + 1), x",
      "Derivata", "D[sqrt(x^2+1)] = x/sqrt(x^2+1)" },
    { "exp(sin(x)), x",
      "Derivata", "Catena doppia e^{sin(x)}" },
    { "log(x + sqrt(x^2 + 1)), x",
      "Derivata", "D[arcsinh(x)] = 1/sqrt(x^2+1)" },
    { "x^2*exp(-x), x",
      "Derivata", "Prodotto polinomio x esponenziale" },
    /* ── Integrali (10) ──────────────────────────────────── */
    { "x*exp(x), x",
      "Integrale", "Per parti: Int x*e^x dx = (x-1)*e^x + C [TEST]" },
    { "sin(x), x, 0, pi",
      "Integrale", "Definito esatto: Int_0^pi sin(x)dx = 2 [TEST]" },
    { "exp(-x^2), x, 0, oo",
      "Integrale", "Gaussiano: Int_0^oo e^{-x^2}dx = sqrt(pi)/2 [TEST]" },
    { "log(x), x",
      "Integrale", "Per parti: Int ln(x)dx = x*ln(x)-x + C" },
    { "x^2*sin(x), x",
      "Integrale", "Per parti iterata su x^2*sin(x)" },
    { "1/(x^2 + 1), x",
      "Integrale", "Int 1/(1+x^2)dx = arctan(x) + C" },
    { "sqrt(1 - x^2), x, -1, 1",
      "Integrale", "Area semicerchio = pi/2" },
    { "sin(x)^2, x",
      "Integrale", "Formula di riduzione: Int sin^2(x)dx" },
    { "1/(x^2 - 1), x",
      "Integrale", "Frazioni parziali: ln|(x-1)/(x+1)|/2 + C" },
    { "x^3*exp(x), x",
      "Integrale", "Per parti ripetuta - 4 passaggi" },
    /* ── Limiti (9) ──────────────────────────────────────── */
    { "sin(x)/x, x, 0",
      "Limite", "Limite notevole fondamentale -> 1 [TEST]" },
    { "(1 - cos(x))/x^2, x, 0",
      "Limite", "Forma 0/0 -> 1/2 (L'Hopital) [TEST]" },
    { "(x^2 + 1)/(x^2 - 1), x, oo",
      "Limite", "Limite all'inf di razionale -> 1 [TEST]" },
    { "(exp(x) - 1)/x, x, 0",
      "Limite", "Limite notevole (e^x-1)/x -> 1" },
    { "(1 + 1/x)^x, x, oo",
      "Limite", "Definizione di e: (1+1/x)^x -> e" },
    { "x*log(x), x, 0",
      "Limite", "Forma 0*oo -> 0" },
    { "(sqrt(x + 1) - 1)/x, x, 0",
      "Limite", "Forma 0/0 -> 1/2 (razionalizzazione)" },
    { "(x^3 - 8)/(x - 2), x, 2",
      "Limite", "Forma 0/0 - fattorizza cubo -> 12" },
    { "sin(3*x)/sin(5*x), x, 0",
      "Limite", "Rapporto seni con limiti notevoli -> 3/5" },
    /* ── Semplificazioni (8) ─────────────────────────────── */
    { "sin(x)^2 + cos(x)^2",
      "Semplificazione", "Identita' di Pitagora = 1 [TEST]" },
    { "(x^3 - 1)/(x - 1)",
      "Semplificazione", "Differenza cubi -> x^2+x+1 [TEST]" },
    { "series(exp(x), x, 0, 6)",
      "Semplificazione", "Taylor di e^x in 0 all'ordine 5 [TEST]" },
    { "factor(x^4 - 5*x^2 + 4)",
      "Semplificazione", "Fattorizzazione quartica" },
    { "expand((x + 1)^6)",
      "Semplificazione", "Binomio di Newton ordine 6" },
    { "simplify(tan(x)^2 + 1 - 1/cos(x)^2)",
      "Semplificazione", "Identita' sec^2(x) = tan^2(x)+1 -> 0" },
    { "series(sin(x), x, 0, 8)",
      "Semplificazione", "Taylor di sin(x) all'ordine 7" },
    { "factor(x^6 - 1)",
      "Semplificazione", "Fattorizzazione differenza sesta potenza" },
};
static const int kNSolveExamples =
    static_cast<int>(sizeof(kSolveExamples)/sizeof(kSolveExamples[0]));


QWidget* MatematicaPage::buildSolveTab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(12, 10, 12, 10);
    lay->setSpacing(8);

    lay->addWidget(new QLabel(
        "<b>Risolvi un'equazione, disequazione o espressione PASSO PER PASSO (stile Derive):</b>", w));

    /* Riga input + tipo */
    auto* inputRow = new QHBoxLayout;
    m_solveInput = new QLineEdit(w);
    m_solveInput->setPlaceholderText(
        "Inserisci equazione o espressione: es. x\xc2\xb2 + 5x + 6 = 0,  2x > 4,  sin(x)/x");
    m_solveInput->setMinimumWidth(320);
    inputRow->addWidget(m_solveInput, 1);

    m_solveCmb = new QComboBox(w);
    m_solveCmb->setObjectName("settingCombo");
    m_solveCmb->addItem("Equazione",       "equazione");
    m_solveCmb->addItem("Disequazione",    "disequazione");
    m_solveCmb->addItem("Derivata",        "derivata");
    m_solveCmb->addItem("Integrale",       "integrale");
    m_solveCmb->addItem("Limite",          "limite");
    m_solveCmb->addItem("Semplificazione", "semplificazione");
    inputRow->addWidget(m_solveCmb);

    auto* btnRandom = new QPushButton("\xf0\x9f\x94\x80", w);  /* 🔀 */
    btnRandom->setObjectName("navBtn");
    btnRandom->setFixedWidth(dpiScale(34));
    btnRandom->setToolTip(
        QString("Formula casuale (%1 disponibili) — cambia tipo ed espressione")
        .arg(kNSolveExamples));
    connect(btnRandom, &QPushButton::clicked,
            this, &MatematicaPage::onSolveRandomClicked);
    inputRow->addWidget(btnRandom);

    auto* btnCopyInput = new QPushButton("\xf0\x9f\x93\x8b", w);  /* 📋 */
    btnCopyInput->setObjectName("navBtn");
    btnCopyInput->setFixedWidth(dpiScale(34));
    btnCopyInput->setToolTip(tr("Copia la formula negli appunti"));
    connect(btnCopyInput, &QPushButton::clicked, m_solveInput, [this]() {
        const QString t = m_solveInput->text();
        if (!t.isEmpty())
            QApplication::clipboard()->setText(t);
    });
    inputRow->addWidget(btnCopyInput);

    lay->addLayout(inputRow);

    /* Barra pulsanti */
    auto* btnRow = new QHBoxLayout;

    m_btnSolve = new QPushButton(
        "\xf0\x9f\x94\xa2  Risolvi passo per passo", w);   /* 🔢 */
    m_btnSolve->setObjectName("actionBtn");
    m_btnSolve->setProperty("highlight", "true");
    connect(m_btnSolve, &QPushButton::clicked, this, &MatematicaPage::onSolveClicked);
    btnRow->addWidget(m_btnSolve);

    auto* btnStop = new QPushButton("\xe2\x96\xa0  Stop", w);  /* ■ */
    btnStop->setObjectName("stopBtn");
    connect(btnStop, &QPushButton::clicked, this, &MatematicaPage::onSolveStopClicked);
    btnRow->addWidget(btnStop);

    m_btnSolveAi = new QPushButton("\xf0\x9f\xa4\x96  Spiega con AI", w);  /* 🤖 */
    m_btnSolveAi->setObjectName("actionBtn");
    m_btnSolveAi->setToolTip(tr("Usa l'LLM selezionato per spiegare i passi SymPy in italiano"));
    m_btnSolveAi->setVisible(false);
    connect(m_btnSolveAi, &QPushButton::clicked, this, &MatematicaPage::onSolveAiClicked);
    btnRow->addWidget(m_btnSolveAi);

    m_btnSolveCopy = new QPushButton("\xf0\x9f\x93\x8b  Copia", w);  /* 📋 */
    m_btnSolveCopy->setObjectName("actionBtn");
    connect(m_btnSolveCopy, &QPushButton::clicked, this, &MatematicaPage::onSolveCopyClicked);
    btnRow->addWidget(m_btnSolveCopy);

    btnRow->addStretch(1);
    lay->addLayout(btnRow);

    /* Nota informativa */
    auto* note = new QLabel(
        "<small><b>Notazione:</b> x^2 per x\xc2\xb2, sqrt(x) per \xe2\x88\x9ax, "
        "sin/cos/tan/log/exp. "
        "Derivata: <i>f(x), var[, ordine]</i> \xe2\x80\x94 "
        "Integrale: <i>f(x)[, a, b]</i> \xe2\x80\x94 "
        "Limite: <i>f(x), var, val</i>. "
        "Il risultato appare nel pannello in basso.</small>", w);
    note->setWordWrap(true);
    lay->addWidget(note);
    return w;
}

/* ══════════════════════════════════════════════════════════════
   onSolveClicked — avvia la risoluzione passo per passo
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::onSolveClicked()
{
    if (m_solveBusy) return;

    const QString expr = m_solveInput->text().trimmed();
    if (expr.isEmpty()) {
        appendOutput("\xe2\x9d\x8c  Inserisci un'equazione o espressione prima di procedere.\n");
        LogBus::post("\xe2\x9d\x8c Matematica: Inserisci un'equazione o espressione prima di procedere.");
        return;
    }

    clearOutput();
    m_solveFullText.clear();
    if (m_btnSolveAi) m_btnSolveAi->setVisible(false);
    setStatus("\xf0\x9f\x93\x90  SymPy in calcolo...");

    const QString tipo = m_solveCmb->currentText();

    /* runPython chiama stopPython() internamente che azzera m_solvePyMode/m_solveBusy;
       le reimpostiamo DOPO la chiamata (il processo gira in modo asincrono). */
    runPython(buildSympyScript(tipo, expr));
    m_solvePyMode = true;
    m_solveBusy   = true;
    m_btnSolve->setEnabled(false);
}

/* ══════════════════════════════════════════════════════════════
   onSolveStopClicked — interrompe il calcolo (SymPy o AI)
   ══════════════════════════════════════════════════════════════ */
/* ══════════════════════════════════════════════════════════════
   onSolveRandomClicked — pesca una formula casuale dall'archivio
   kSolveExamples e la inserisce nel campo + imposta il tipo.
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::onSolveRandomClicked()
{
    const int idx = static_cast<int>(
        QRandomGenerator::global()->bounded(static_cast<quint32>(kNSolveExamples)));
    const SolveExample& ex = kSolveExamples[idx];

    if (m_solveInput)
        m_solveInput->setText(QString::fromUtf8(ex.expr));

    if (m_solveCmb) {
        const int ci = m_solveCmb->findText(QString::fromUtf8(ex.tipo));
        if (ci >= 0) m_solveCmb->setCurrentIndex(ci);
    }

    setStatus(QString("\xf0\x9f\x94\x80  %1: %2")   /* 🔀 */
              .arg(QString::fromUtf8(ex.tipo),
                   QString::fromUtf8(ex.desc)));
}

void MatematicaPage::onSolveStopClicked()
{
    if (!m_solveBusy) return;
    if (m_solvePyMode) {
        stopPython();   /* resetta m_solvePyMode e m_solveBusy */
    } else {
        if (m_ai) m_ai->abort();
        delete m_aiSolveHolder;
        m_aiSolveHolder = nullptr;
        m_solveBusy = false;
        if (m_btnSolve)   m_btnSolve->setEnabled(true);
        if (m_btnSolveAi) m_btnSolveAi->setVisible(true);
    }
    setStatus("\xe2\x96\xa0  Risoluzione interrotta.");
}

/* ══════════════════════════════════════════════════════════════
   onSolveAiClicked — spiega il risultato SymPy con l'LLM selezionato
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::onSolveAiClicked()
{
    if (m_solveBusy || !m_ai) return;
    const QString sympyOut = m_solveFullText.trimmed();
    if (sympyOut.isEmpty()) return;

    /* Applica il modello scelto nella combo condivisa */
    if (m_modelCombo) {
        const QString sel = m_modelCombo->currentData().toString();
        if (!sel.isEmpty() && sel != m_ai->model())
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), sel);
    }

    /* ── Hint dominio: suggerisci modello math-capable se la query è STEM ── */
    if (m_solveInput) {
        const QString hint = buildDomainHint(m_ai, m_solveInput->text(), m_modelCombo);
        if (!hint.isEmpty())
            appendOutput(hint + "\n\n");
    }

    m_solveBusy = true;
    m_btnSolve->setEnabled(false);
    if (m_btnSolveAi) m_btnSolveAi->setEnabled(false);
    setStatus("\xf0\x9f\xa4\x96  AI sta spiegando...");

    appendOutput("\n\n\xe2\x94\x80\xe2\x94\x80 \xf0\x9f\xa4\x96 Spiegazione AI "
                 "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\n");

    const QString sys =
        "Sei un professore di matematica universitaria. Ricevi l'output di SymPy "
        "(calcolo simbolico esatto) e devi commentarlo in italiano con rigore didattico.\n\n"
        "PER OGNI PASSO dell'output devi rispondere a TRE domande:\n"
        "  1. COSA è stato fatto (brevemente — lo studente lo vede già).\n"
        "  2. PERCHÉ questo passo è necessario in questo momento — "
        "qual è la motivazione matematica, il teorema o il principio che lo giustifica.\n"
        "  3. COSA sarebbe successo se non lo avessimo fatto, o quale alternativa "
        "esisteva e perché non è stata scelta.\n\n"
        "Regole di stile:\n"
        "- Scrivi in prosa fluente, non in elenchi puntati.\n"
        "- Cita i teoremi per nome quando rilevante "
        "(es. 'Teorema Fondamentale del Calcolo', 'Regola di L\\'Hôpital').\n"
        "- NON ripetere i calcoli simbolici — commentali concettualmente.\n"
        "- Chiudi con una verifica o un'osservazione pratica utile allo studente.\n"
        "Rispondi ESCLUSIVAMENTE in italiano.";

    const QString user = QString(
        "Problema: %1\n\n"
        "Output SymPy (passi già eseguiti con motivazioni brevi):\n%2\n\n"
        "Approfondisci le motivazioni di ogni passo come descritto nel tuo ruolo.")
        .arg(m_solveInput->text(), sympyOut);

    delete m_aiSolveHolder;
    m_aiSolveHolder = new QObject(this);
    connect(m_ai, &AiClient::token,    m_aiSolveHolder,
            [this](const QString& t){ onSolveToken(t); });
    connect(m_ai, &AiClient::finished, m_aiSolveHolder,
            [this](const QString& f){ onSolveFinished(f); });
    connect(m_ai, &AiClient::error,    m_aiSolveHolder,
            [this](const QString& e){ onSolveError(e); });

    m_ai->chat(P::prependMathKnowledge(sys), user);
}

/* ══════════════════════════════════════════════════════════════
   onSolveCopyClicked — copia il testo dell'output negli appunti
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::onSolveCopyClicked()
{
    const QString txt = m_output->toPlainText();
    if (txt.isEmpty()) return;
    QApplication::clipboard()->setText(txt);
    if (m_btnSolveCopy) m_btnSolveCopy->setText(tr("\xe2\x9c\x85  Copiato!"));
    QTimer::singleShot(1500, this, &MatematicaPage::onSolveRestoreCopyBtn);
}

/* ══════════════════════════════════════════════════════════════
   onSolveRestoreCopyBtn — ripristina il testo del pulsante Copia
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::onSolveRestoreCopyBtn()
{
    if (m_btnSolveCopy)
        m_btnSolveCopy->setText(tr("\xf0\x9f\x93\x8b  Copia"));
}

/* ══════════════════════════════════════════════════════════════
   Slot AI — streaming token
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::onSolveToken(const QString& t)
{
    appendOutput(t);
}

/* ══════════════════════════════════════════════════════════════
   Slot AI — risposta completa
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::onSolveFinished(const QString& full)
{
    if (m_latexOut && m_latexOut->isVisible() && !full.isEmpty())
        m_latexOut->setLatexHtml(
            "<div class='ai-out'>" + full.toHtmlEscaped().replace("\n", "<br>") + "</div>");
    delete m_aiSolveHolder;
    m_aiSolveHolder = nullptr;
    m_solveBusy = false;
    if (m_btnSolve)   m_btnSolve->setEnabled(true);
    if (m_btnSolveAi) { m_btnSolveAi->setEnabled(true); m_btnSolveAi->setVisible(true); }
    setStatus("\xe2\x9c\x85  Spiegazione AI completata.");
}

/* ══════════════════════════════════════════════════════════════
   Slot AI — errore
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::onSolveError(const QString& msg)
{
    delete m_aiSolveHolder;
    m_aiSolveHolder = nullptr;
    m_solveBusy = false;
    if (m_btnSolve)   m_btnSolve->setEnabled(true);
    if (m_btnSolveAi) { m_btnSolveAi->setEnabled(true); m_btnSolveAi->setVisible(true); }
    appendOutput("\n\xe2\x9d\x8c  Errore AI: " + msg + "\n");
    setStatus("\xe2\x9d\x8c  Errore AI.");
    LogBus::post("\xe2\x9d\x8c Matematica: Errore AI risoluzione: " + msg);
}

/* ══════════════════════════════════════════════════════════════
   Dati statici argomenti Analisi 1 e 2
   ══════════════════════════════════════════════════════════════ */

