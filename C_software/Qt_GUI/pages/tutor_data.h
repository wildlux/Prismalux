#pragma once
#include <QString>
#include <QVector>
#include <QPair>

using TutorTopic   = QPair<QString,QString>;               // {nome, descrizione}
using TutorSection = QPair<QString,QVector<TutorTopic>>;   // {titolo_sezione, argomenti}
using SubjectData  = QVector<TutorSection>;

SubjectData tutorDataMatematica();
SubjectData tutorDataFisica();
SubjectData tutorDataChimica();
SubjectData tutorDataSicurezza();
SubjectData tutorDataInformatica();
SubjectData tutorDataAlgoritmi();
SubjectData tutorDataBiologia();
SubjectData tutorDataAstronomia();
SubjectData tutorDataElettronica();
SubjectData tutorDataEconomia();
SubjectData tutorDataFarmacologia();
SubjectData tutorDataQuantumComputing();
