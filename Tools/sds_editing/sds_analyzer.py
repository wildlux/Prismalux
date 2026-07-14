#!/usr/bin/env python3
# =============================================================================
# ⚠️  STRUMENTO DIDATTICO — NON PRODUCE RISULTATI CLINICI
# Genera solo grafici dell'andamento degli esami del sangue a partire da un CSV.
# Le soglie sono indicative: NON sostituiscono l'interpretazione di un medico.
# =============================================================================
"""
sds_analyzer.py - Analisi degli esami del sangue + report PDF
"""

import pandas as pd
import matplotlib.pyplot as plt
from datetime import datetime
import os
import json
import logging
from reportlab.lib.pagesizes import letter
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Image
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib import colors
from reportlab.lib.units import inch

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


def analyze_blood_tests(csv_file):
    """Analizza il file CSV con gli esami del sangue."""
    if not os.path.exists(csv_file):
        logger.warning(f"File {csv_file} non trovato. Creo un file di esempio.")
        df = pd.DataFrame({
            'data': pd.date_range(start='2023-01-01', periods=12, freq='M'),
            'globuli_bianchi': [3.5, 3.2, 2.8, 2.5, 2.2, 2.0, 1.8, 2.1, 2.4, 2.7, 3.0, 3.3],
            'emoglobina': [12.5, 12.0, 11.5, 11.0, 10.5, 10.0, 9.5, 10.2, 10.8, 11.5, 12.0, 12.5],
            'piastrine': [250, 230, 210, 190, 170, 150, 130, 150, 170, 190, 210, 230],
            'neutrofili': [2.5, 2.2, 1.8, 1.5, 1.2, 1.0, 0.8, 1.1, 1.4, 1.7, 2.0, 2.3]
        })
        df.to_csv(csv_file, index=False)
        logger.info(f"File esempio creato: {csv_file}")

    df = pd.read_csv(csv_file, parse_dates=['data'])
    df = df.sort_values('data')

    soglie = {
        'globuli_bianchi': {'min': 2.0, 'max': 4.0, 'unita': 'x10^3/uL', 'nome': 'Globuli Bianchi'},
        'emoglobina': {'min': 10.0, 'max': 14.0, 'unita': 'g/dL', 'nome': 'Emoglobina'},
        'piastrine': {'min': 100, 'max': 300, 'unita': 'x10^3/uL', 'nome': 'Piastrine'},
        'neutrofili': {'min': 1.0, 'max': 7.0, 'unita': 'x10^3/uL', 'nome': 'Neutrofili'}
    }

    fig, axes = plt.subplots(len(soglie), 1, figsize=(10, 5 * len(soglie)))
    if len(soglie) == 1:
        axes = [axes]

    alerts = []
    for i, (param, limits) in enumerate(soglie.items()):
        ax = axes[i]
        if param in df.columns:
            ax.plot(df['data'], df[param], marker='o', linestyle='-', linewidth=2)
            ax.axhline(limits['min'], color='red', linestyle='--', linewidth=1)
            ax.axhline(limits['max'], color='orange', linestyle='--', linewidth=1)
            ax.set_ylabel(f"{limits['nome']} ({limits['unita']})")
            ax.set_xlabel('Data')
            ax.grid(True, alpha=0.3)
            ultimo = df[param].iloc[-1]
            if ultimo < limits['min']:
                alerts.append(f"[!] {limits['nome']} = {ultimo} {limits['unita']} (SOTTO soglia)")
            elif ultimo > limits['max']:
                alerts.append(f"[!] {limits['nome']} = {ultimo} {limits['unita']} (SOPRA soglia)")
            else:
                alerts.append(f"[ok] {limits['nome']} = {ultimo} {limits['unita']} (Normale)")

    plt.tight_layout()
    plot_path = 'blood_trends.png'
    plt.savefig(plot_path, dpi=150)
    plt.close()
    return df, alerts, plot_path


def generate_pdf_report(alerts, plot_path, output_pdf):
    """Genera il report PDF."""
    doc = SimpleDocTemplate(output_pdf, pagesize=letter)
    styles = getSampleStyleSheet()
    story = []

    title_style = ParagraphStyle('TitleStyle', parent=styles['Title'], fontSize=24, textColor=colors.darkblue)
    story.append(Paragraph("Report - Andamento esami (uso personale, non clinico)", title_style))
    story.append(Spacer(1, 0.3 * inch))
    story.append(Paragraph(f"Data: {datetime.now().strftime('%d/%m/%Y %H:%M')}", styles['Normal']))
    story.append(Spacer(1, 0.2 * inch))
    story.append(Paragraph(
        "Nota: soglie indicative. L'interpretazione spetta al medico.", styles['Italic']))
    story.append(Spacer(1, 0.2 * inch))

    story.append(Paragraph("Andamento esami del sangue", styles['Heading2']))
    for msg in alerts:
        story.append(Paragraph(msg, styles['Normal']))
    if os.path.exists(plot_path):
        story.append(Image(plot_path, width=6 * inch, height=4 * inch))

    doc.build(story)
    logger.info(f"Report PDF generato: {output_pdf}")


if __name__ == "__main__":
    config = json.load(open("config.json"))
    df, alerts, plot_path = analyze_blood_tests(config["blood_csv"])
    generate_pdf_report(alerts, plot_path, config["output_pdf"])
