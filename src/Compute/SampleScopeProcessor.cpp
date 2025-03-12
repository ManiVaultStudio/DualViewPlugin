#include "SampleScopeProcessor.h"

namespace 
{
	const int fontSize = 13;
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
	QString html = "<html><head><style>"
		"body { font-family: Arial; }"
		".legend { list-style: none; padding: 0; }"
		".legend li { display: flex; align-items: center; margin-bottom: 5px; }"
		".legend-color { width: 16px; height: 16px; display: inline-block; margin-right: 5px; }"
		"</style></head><body>";

	// show first 80 genes and the rest in expandable box
	QStringList geneSymbolsLessThan80;
	QStringList additionalSymbols;

	bool hasMoreThan80;

	if (geneSymbols.size() > 80)
	{
		hasMoreThan80 = true;
		geneSymbolsLessThan80 = geneSymbols.mid(0, 80);
		additionalSymbols = geneSymbols.mid(80);
		qDebug() << "geneSymbols.size: " << geneSymbols.size();
		qDebug() << "geneSymbolsLessThan80.size: " << geneSymbolsLessThan80.size();
		qDebug() << "additionalSymbols.size: " << additionalSymbols.size();
	}
	else
	{
		hasMoreThan80 = false;
		geneSymbolsLessThan80 = geneSymbols;
		qDebug() << "geneSymbols.size: " << geneSymbols.size();
	}

	QString outputText;
	QString chartTitle;
	QString additionalGenesHtml;

	if (hasMoreThan80)
	{
		additionalGenesHtml = QString(
			"<details><summary style='font-size:%1px; cursor:pointer;'>and more... </summary>"
			"<p style='font-size:%1px;'>%2</p>"
			"</details>")
			.arg(fontSize)
			.arg(additionalSymbols.join(", "));
	}

	if (isASelected) {
		outputText = QString(
			"<p style='font-size:%1px;'>Gene embedding is selected.</p>"
			"<table style='font-size:%1px;'>"
			"<tr>"
			"<td><b>Selected Genes: </b></td>"
			"<td>%2 %3</td>"  // add the expandable section right inside the same <td>
			"</tr>"
			"</table>"
			"<p style='font-size:12px; color:#377fe4;'>" // TODO: hard coded font here
			"Click the 'Enrich' button in the gene panel for more details.</p>")
			.arg(fontSize)
			.arg(geneSymbolsLessThan80.join(", "))
			.arg(additionalGenesHtml);  // append the expandable section here

		chartTitle = "<b>Connected cell proportion (10% highest expression of avg. selected genes)</b>";
	}
	else {
		outputText = QString(
			"<p style='font-size:%1px;'>Cell embedding is selected.</p>"
			"<table style='font-size:%1px;'>"
			"<tr>"
			"<td><b>Connected Genes: </b></td>"
			"<td>%2 %3</td>"  // add the expandable section right inside the same <td>
			"</tr>"
			"</table>"
			"<p style='font-size:12px; color:#377fe4;'>"// TODO: hard coded font here
			"Click the 'Enrich' button in the gene panel for more details.</p>")
			.arg(fontSize)
			.arg(geneSymbolsLessThan80.join(", "))
			.arg(additionalGenesHtml);

		chartTitle = "<b>Cell proportion</b>";
	}

	// get current color dataset B name
	QString colorDatasetIntro;

	//if (colorDatasetID.isEmpty())
	if (colorDatasetName.isEmpty())
	{
		html += "<p>" + outputText + "</p>";
	}
	else
	{
		//colorDatasetName = mv::data().getDataset(colorDatasetID)->getGuiName();
		colorDatasetIntro = QString("Cell embedding coloring by %1").arg(colorDatasetName);

		QVector<double> counts;

		// Convert data to numbers and calculate total sum
		double totalCount = 0.0;
		for (const QVariant& dataVar : data) {
			double value = dataVar.toDouble();
			counts.append(value);
			totalCount += value;
		}

		html += outputText; // font size al set in the table style
		html += QString("<p style='font-size:%1px;'>%2</p>")
			.arg(fontSize)
			.arg(colorDatasetIntro);
		html += QString("<p style='font-size:%1px;'>%2</p>")
			.arg(fontSize)
			.arg(chartTitle);

		// dimensions for the stacked bar
		int barWidth = 50;
		int barHeight = 200;

		int svgWidth = barWidth + 300;

		html += QString("<svg width='%1' height='%2' style='border:none;'>")
			.arg(svgWidth)
			.arg(barHeight);

		if (counts.size() == 1) {
			// if there's only one category, fill the entire bar
			html += QString("<rect x='0' y='0' width='%1' height='%2' fill='%3' stroke='none'></rect>")
				.arg(barWidth)
				.arg(barHeight)
				.arg(backgroundColors[0]);

			double labelX = barWidth + 5;
			double labelY = barHeight / 2.0;

			html += QString(
				"<text x='%1' y='%2' font-family='Arial' font-size='12' " // TODO: hard coded font here
				"fill='black' text-anchor='start' alignment-baseline='middle'>"
				"%3</text>")
				.arg(labelX)
				.arg(labelY)
				.arg(labels[0]);
		}
		else {
			// if multiple categories, stack them vertically
			double yOffset = 0.0;

			for (int i = 0; i < counts.size(); ++i) {
				double proportion = counts[i] / totalCount;
				double sliceHeight = proportion * barHeight;

				html += QString("<rect x='0' y='%1' width='%2' height='%3' " "fill='%4' stroke='white' stroke-width='1'></rect>")
					.arg(yOffset)
					.arg(barWidth)
					.arg(sliceHeight)
					.arg(backgroundColors[i]);

				// whether to show labels
				double minPixelsForLabel = 10.0;
				if (sliceHeight >= minPixelsForLabel)
				{
					double labelX = barWidth + 5;
					double labelY = yOffset + (sliceHeight / 2.0);

					double percentage = (counts[i] / totalCount) * 100.0;

					QString labelText = QString("%1 (%2%)").arg(labels[i])
						.arg(QString::number(percentage, 'f', 1));

					html += QString(
						"<text x='%1' y='%2' font-family='Arial' font-size='12' "// TODO: hard coded font here
						"fill='black' text-anchor='start' alignment-baseline='middle'>"
						"%3</text>")
						.arg(labelX)
						.arg(labelY)
						.arg(labelText);
				}
				yOffset += sliceHeight;
			}
		}
		html += "</svg>";
	}

	return html;
}

QString buildHtmlForEnrichmentResults(const QVariantList& data)
{
	// Manually define headers
	QStringList headers = { "Source", "Term ID", "Term Name", "Padj", "Highlighted", "Symbol" };

	// Limit data to max 10 rows
	int maxRows = qMin(30, data.size());
	QVariantList limitedData;
	for (int i = 0; i < maxRows; ++i) {
		limitedData.append(data[i]);
	}

	QString tableHtml = QString("<p style='font-size:%1px;'>Enrichment Analysis Results</p>").arg(fontSize);

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

		for (const QString& key : headers) {
			QString value = dataMap[key].toString();

			// format Padj 3 decimal places
			if (key == "Padj") {
				bool ok;
				double pValue = value.toDouble(&ok);
				if (ok) {
					value = QString::number(pValue, 'e', 3);
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
	QString tableHtml = "<p style='font-size:14px;'>Enrichment Analysis Results</p>";

	tableHtml += "<table style='font-size:14px; border-collapse: collapse; width:100%; font-family: Arial;' border='1'>"
		"<tr>"
		"<th style='padding: 5px; width: auto;'>No GO term available</th>"
		"</tr>"
		"</table>";

	return tableHtml;
}