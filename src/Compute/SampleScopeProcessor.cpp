#include "SampleScopeProcessor.h"

namespace 
{
	const int fontSize = 12;
}


std::tuple<QStringList, QStringList, QStringList> computeMetadataCounts(QVector<Cluster>& metadata, std::vector<std::uint32_t>& sampledPoints)
{
    QStringList labels;
    QStringList data;
    QStringList backgroundColors;

    std::unordered_map<int, int> cellToClusterMap; // maps global cell index to cluster index
    for (int clusterIndex = 0; clusterIndex < metadata.size(); ++clusterIndex) {
        const auto& ptIndices = metadata[clusterIndex].getIndices();
        for (int cellIndex : ptIndices) {
            cellToClusterMap[cellIndex] = clusterIndex;
        }
    }

    std::unordered_map<int, int> clusterCount; // maps cluster index to count
    for (int i = 0; i < sampledPoints.size(); ++i) {
        int globalCellIndex = sampledPoints[i];
        if (cellToClusterMap.find(globalCellIndex) != cellToClusterMap.end()) {
            int clusterIndex = cellToClusterMap[globalCellIndex];
            clusterCount[clusterIndex]++;
        }
    }

    std::vector<std::pair<int, int>> sortedClusterCount(clusterCount.begin(), clusterCount.end());
    std::sort(sortedClusterCount.begin(), sortedClusterCount.end(), [](const auto& a, const auto& b) { return a.second > b.second; });

    for (const auto& [clusterIndex, count] : sortedClusterCount) 
    {
        QString clusterName = metadata[clusterIndex].getName();
        labels << clusterName;
        data << QString::number(count);

        QString color = metadata[clusterIndex].getColor().name();
        backgroundColors << color;
    }

    return { labels, data, backgroundColors };
}

QString buildHtmlForSelection(const bool isASelected, const QString colorDatasetName, const QStringList& geneSymbols, QStringList& labels, QStringList& data, QStringList& backgroundColors)
{

	QString html = QString(R"(
        <html><head><style>
            body { font-family: Arial; font-size: %1px; }
            .info-table { width: 100%; border-collapse: collapse; font-size: %1px;}
            .info-table td { padding: 6px 8px 12px 0; vertical-align: top; font-size: %1px;}
            .label-col { font-weight: bold; width: 25%; min-width: 140px; max-width: 220px; }
            .value-col { width: 75%; }
            .hint-text { font-size: 0.9em; margin-top: 6px; display: block; }
            .action-text { font-size: 0.9em; color: #0066cc; margin-top: 6px; display: block; }
            .legend { list-style: none; padding: 0; margin-top: 8px; margin-bottom: 0; }
            .legend li { display: inline-block; margin-right: 15px; margin-bottom: 5px; font-size: %1px; }
            .legend-color { width: 14px; height: 14px; display: inline-block; margin-right: 5px; vertical-align: middle; border: 1px solid #ccc;}
            summary { font-size: %1px; cursor: pointer; color: #0066cc; margin-top: 4px; }
        </style></head><body>
        <table class='info-table'>
    )").arg(fontSize);

	// ---------------------------------------------------------
	// ROW 1: Current views
	// ---------------------------------------------------------
	QString viewStatus = isASelected ? "Gene embedding is selected." : "Cell embedding is selected.";
	if (!colorDatasetName.isEmpty()) {
		viewStatus += QString(" Cell embedding is colored by '%1'.").arg(colorDatasetName);
	}

	html += QString("<tr><td class='label-col'>Current views:</td>"
		"<td class='value-col'>%1</td></tr>").arg(viewStatus);


	// ---------------------------------------------------------
	// ROW 2: Selected/Connected genes
	// ---------------------------------------------------------
	QString geneLabel = isASelected ? "Selected genes:" : "Connected genes:";
	QString genesHtml = "";

	int limit = 80;
	QStringList genesToDisplay = geneSymbols.mid(0, limit);
	genesHtml += genesToDisplay.join(", ");

	if (geneSymbols.size() > limit)
	{
		QStringList additionalSymbols = geneSymbols.mid(limit);
		genesHtml += QString(
			"<details><summary>and %1 more... (click to expand)</summary>"
			"<p style='margin-top:4px;'>%2</p></details>")
			.arg(additionalSymbols.size())
			.arg(additionalSymbols.join(", "));
	}

	//genesHtml += "<span class='action-text'>Click the 'Enrich' button in the gene panel for more details.</span>";

	html += QString("<tr><td class='label-col'>%1</td>"
		"<td class='value-col'>%2</td></tr>")
		.arg(geneLabel)
		.arg(genesHtml);

	// ---------------------------------------------------------
	// ROW 3: Cell type proportions (Only if colorDatasetName exists)
	// ---------------------------------------------------------
	if (!colorDatasetName.isEmpty() && !data.isEmpty())
	{
		QString chartTitle = isASelected ?
			"Connected cell proportion:<span class='hint-text'>(1% highest expression)</span>" :
			"Cell proportion:";

		html += QString("<tr><td class='label-col'>%1</td><td class='value-col'>").arg(chartTitle);

		// Convert data to numbers and calculate total sum
		double totalCount = 0.0;
		QVector<double> counts;
		for (const QVariant& dataVar : data) {
			double value = dataVar.toDouble();
			counts.append(value);
			totalCount += value;
		}

		if (totalCount > 0)
		{
			// HORIZONTAl stacked bar
			int svgWidth = 300; 
			int svgHeight = 24; 
			double xOffset = 0.0;

			// Define a cutoff threshold. 
		    // 0.05% rounds to 0.1%. Anything below 0.05% formats as 0.0%.
			const double CUTOFF_PERCENT = 0.05;

			html += QString("<svg width='%1' height='%2' style='border:none; border-radius:3px;'>")
				.arg(svgWidth).arg(svgHeight);

			QString legendHtml = "<ul class='legend'>";

			for (int i = 0; i < counts.size(); ++i) {
				double proportion = counts[i] / totalCount;
				double percentage = proportion * 100.0;

				// Skip this category if it's too small to show
				if (percentage < CUTOFF_PERCENT) {
					continue;
				}

				double sliceWidth = proportion * svgWidth;

				html += QString("<rect x='%1' y='0' width='%2' height='%3' fill='%4' stroke='white' stroke-width='1'></rect>")
					.arg(xOffset)
					.arg(sliceWidth)
					.arg(svgHeight)
					.arg(backgroundColors[i]);

				xOffset += sliceWidth;

				legendHtml += QString(
					"<li><span class='legend-color' style='background-color:%1;'></span>%2 (%3%)</li>")
					.arg(backgroundColors[i])
					.arg(labels[i])
					.arg(QString::number(percentage, 'f', 1));
			}
			html += "</svg>";
			html += legendHtml + "</ul>";
		}
		html += "</td></tr>";
	}

	html += "</table></body></html>";

	return html;
}

QString buildHtmlForEnrichmentResults(const QVariantList& data)
{
	// Manually define headers
	QStringList headers = { "Source", "Term ID", "Term Name", "Padj", "Highlight", "Symbol" };

	// Limit data to max X rows
	int maxRows = qMin(30, data.size());
	QVariantList limitedData;
	for (int i = 0; i < maxRows; ++i) {
		limitedData.append(data[i]);
	}

	QString tableHtml = QString("<p style='font-size:%1px; font-weight:bold;'>Enrichment analysis results</p>").arg(fontSize);

	tableHtml += QString("<table style='font-size:%1px; border-collapse: collapse; width:100%; font-family: Arial;' border='1'>"
		"<tr>")
		.arg(fontSize);

	// Add headers with fixed width for better alignment
	for (const QString& header : headers) {
		tableHtml += "<th style='padding: 5px; width: auto;'>" + header + "</th>";
	}
	tableHtml += "</tr>";

	for (const QVariant& item : limitedData) {
		QVariantMap dataMap = item.toMap();
		QString termID = dataMap["Term ID"].toString();

		//tableHtml += "<tr>";

		// enable click on row
		tableHtml += QString("<tr onclick='rowClicked(\"%1\")' style='cursor:pointer;'>").arg(termID);

		for (const QString& key : headers) 
		{
			QString value = dataMap[key].toString();

			// format Padj 3 decimal places
			if (key == "Padj") {
				bool ok;
				double pValue = value.toDouble(&ok);
				if (ok) {
					value = QString::number(pValue, 'e', 3);
				}
			}

			// collapse long symbol list
			if (key == "Symbol")
			{
				QStringList genes = value.split(",");
				if (genes.size() > 5)
				{
					QString shortList = genes.mid(0, 5).join(", ");
					QString fullList = genes.join(", ");
					value = QString(
						"%1 <details onclick='event.stopPropagation()' style='display:inline;'>"
						"<summary style='cursor:pointer; display:inline;'>and more... </summary>"
						"<p>%2</p></details>")
						.arg(shortList)
						.arg(fullList);
				}
			}

			tableHtml += "<td style='padding: 5px; text-align: left; width: auto; white-space:nowrap;'>" + value + "</td>";
		}

		tableHtml += "</tr>";
	}

	tableHtml += "</table>";

	// JavaScript for QWebChannel Integration
	tableHtml += "<script src='qrc:///qtwebchannel/qwebchannel.js'></script>"
		"<script>"
		"var qtBridge;"
		"new QWebChannel(qt.webChannelTransport, function(channel) {"
		"   qtBridge = channel.objects.qtBridge;"
		"});"
		"function rowClicked(goTermID) {"
		"   console.log('Clicked GO Term:', goTermID);"
		"   if (qtBridge) {"
		"       qtBridge.js_qt_passSelectionToQt(goTermID);"
		"   }"
		"}"
		"</script>";

	return tableHtml;
}

QString buildHtmlForEmpty()
{
	// show no data message
	QString tableHtml = QString("<p style='font-size:%1px; font-weight:bold;'>Enrichment analysis results</p>").arg(fontSize);

	tableHtml += QString("<table style='font-size:%1px; border-collapse: collapse; width:100%; font-family: Arial;' border='1'>")
		.arg(fontSize)
		+ "<tr>"
		+ "<th style='padding: 5px; width: auto;'>No GO term available</th>"
		+ "</tr>"
		+ "</table>";

	return tableHtml;
}