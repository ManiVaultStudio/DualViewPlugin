#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>

class EnrichmentAnalysis : public QObject
{
    Q_OBJECT

public:
    explicit EnrichmentAnalysis(QObject* parent = nullptr);

    // ToppGene
    void lookupSymbolsToppGene(const QStringList& symbols);
    void postGeneToppGene(const QJsonArray& entrezIds);

    // gProfiler
    void postGeneGprofiler(const QStringList& query, const QStringList& background, const QString& species); // const QStringList& query, const QStringList& background gprofiler

    // gProfiler post GO term ID and get gene list
    void postGOtermGprofiler(const QString& GOTermId, const QString& species);

signals:
    void enrichmentDataReady(const QVariantList& outputList);

    void enrichmentDataNotExists();

    //void genesFromGOtermDataReady(const QVariantList& outputList);
    void genesFromGOtermDataReady(const QStringList& outputList);

private slots:
    // ToppGene
    void handleLookupReplyToppGene();
    void handleEnrichmentReplyToppGene();

    // gProfiler
    void handleEnrichmentReplyGprofiler();

    void handleGOtermReplyGprofiler();

private:
    QNetworkAccessManager* networkManager;  
};