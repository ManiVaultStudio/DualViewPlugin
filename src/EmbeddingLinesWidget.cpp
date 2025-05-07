#include "EmbeddingLinesWidget.h"

#include <QSurfaceFormat>

#include <stdexcept>
#include <util/Exception.h>
#include <unordered_set>

#include <QDebug>
#include <chrono>
#include <graphics/Matrix3f.h>
//#include <QOpenGLDebugLogger>

using namespace mv;

EmbeddingLinesWidget::EmbeddingLinesWidget() :
    _color(QColor(93, 93, 225)), // color for lines
    _alpha(0.001f), // alpha for lines, try 0.001f for large datasets
    _pixelSelectionTool(this),
    _vao(0),
    _vboMode(0),
    _vboPositions(0),
    _lineConnections(0),
    _highlightSource(true),
    _pointRenderer(),
    _colors(),
    _bounds(),
    /*_embedding_src(nullptr),
    _embedding_dst(nullptr),*/
    _initialized(false)
{

    //Configure pixel selection tool
    _pixelSelectionTool.setEnabled(true);
    _pixelSelectionTool.setMainColor(QColor(Qt::black));

    // update the selection tool when the shape changes
    QObject::connect(&_pixelSelectionTool, &PixelSelectionTool::shapeChanged, [this]() {
        if (isInitialized())
            update();
        });

    // process the selection when the user has finished selecting 
    QObject::connect(&_pixelSelectionTool, &PixelSelectionTool::ended, [this]() {
        if (isInitialized())
            update();
        });
}

void EmbeddingLinesWidget::setData(const std::vector<mv::Vector2f>& embedding_src, const std::vector<mv::Vector2f>& embedding_dst) 
{
    _embedding_src = embedding_src;
    _embedding_dst = embedding_dst;

    //qDebug() << "EmbeddingLinesWidget::setData - embedding_src size:" << _embedding_src.size() << "embedding_dst size:" << _embedding_dst.size();

    // Set the data for the point renderer - src first , then dst
    std::vector<mv::Vector2f> points;
    points.reserve(_embedding_src.size() + _embedding_dst.size());
    points.insert(points.end(), _embedding_src.begin(), _embedding_src.end());
    points.insert(points.end(), _embedding_dst.begin(), _embedding_dst.end());

    const auto numPoints = points.size();

    mv::Vector3f pointColor = { _color.redF(), _color.greenF(), _color.blueF() };
    _colors.clear();
    _colors.reserve(numPoints);
    for (unsigned long i = 0; i < numPoints; i++)
        _colors.emplace_back(pointColor);

    //qDebug() << "EmbeddingLinesWidget::setData - points size:" << points.size();

    _bounds = Bounds::Max;

    for (const Vector2f& point : points)
    {
        _bounds.setLeft(std::min(point.x, _bounds.getLeft()));
        _bounds.setRight(std::max(point.x, _bounds.getRight()));
        _bounds.setBottom(std::min(point.y, _bounds.getBottom()));
        _bounds.setTop(std::max(point.y, _bounds.getTop()));
    }

    //_bounds.ensureMinimumSize(1e-07f, 1e-07f);
    //_bounds.makeSquare();
    //_bounds.expand(0.1f);

    const auto dataBoundsRect = QRectF(QPointF(_bounds.getLeft(), _bounds.getBottom()), QSizeF(_bounds.getWidth(), _bounds.getHeight()));

    _pointRenderer.setDataBounds(dataBoundsRect);
    _pointRenderer.setData(points);
    _pointRenderer.setColors(_colors);

    // line positions
    glBindBuffer(GL_ARRAY_BUFFER, _vboPositions);
    glBufferData(GL_ARRAY_BUFFER, points.size() * sizeof(mv::Vector2f), points.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);


    update();
}

void EmbeddingLinesWidget::setPointColorA(const std::vector<Vector3f>& pointColors)
{
    // pointColors only for A, so only need to set colors for src points
    //qDebug() << "EmbeddingLinesWidget::setPointColorA - pointColors size:" << pointColors.size() << "embedding_dst size:" << _embedding_dst.size();
    //qDebug() << "EmbeddingLinesWidget::setPointColorA - embedding_src size:" << _embedding_src.size() << "embedding_dst size:" << _embedding_dst.size();
    int j = 0;
    for (int i = 0; i < _embedding_src.size(); i++)
    {
        _colors[i] = pointColors[j];
        j++;
    }

    _pointRenderer.setColors(_colors);
    update();

}

void EmbeddingLinesWidget::setPointColorB(const std::vector<Vector3f>& pointColors)
{
    // pointColors only for B, so only need to set colors for dst points
    //qDebug() << "EmbeddingLinesWidget::setPointColorB - pointColors size:" << pointColors.size() << "embedding_dst size:" << _embedding_dst.size();
    //qDebug() << "EmbeddingLinesWidget::setPointColorB - embedding_src size:" << _embedding_src.size() << "embedding_dst size:" << _embedding_dst.size();
    int j = 0;
    for (int i = _embedding_src.size(); i < _embedding_dst.size()+ _embedding_src.size(); i++)
    {
        _colors[i] = pointColors[j];
        j++;
    }
        
    _pointRenderer.setColors(_colors);
	update();

}

void EmbeddingLinesWidget::setLines(const std::vector<std::pair<uint32_t, uint32_t>>& lines) {
    _lines = lines;
    _alpha_per_line.clear();

    // Build index buffer
    // For a line (i, j), i is in src array, j is in dst array
    // The total unique points is _embedding_src.size() + _embedding_dst.size()
    // So the index for a src point is i
    // The index for a dst point is offset by _embedding_src.size()

    const uint32_t srcCount = static_cast<uint32_t>(_embedding_src.size());
    std::vector<GLuint> indices;
    indices.resize(_lines.size() * 2);

#pragma omp parallel for
    for (size_t idx = 0; idx < _lines.size(); ++idx) {
        const auto& line = _lines[idx];
        // line.first = index in src
        // line.second = index in dst
        // Pair: (line.first, srcCount + line.second)

        indices[2 * idx + 0] = line.first;
        indices[2 * idx + 1] = srcCount + line.second;
    }

    // upload the index data to the EBO
    glBindVertexArray(_vao); 
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _lineConnections);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

    // Method 1: clear highlight data - using the 1 byte per vertex to store the highlight data in the mode buffer
    size_t totalVertices = _embedding_src.size() + _embedding_dst.size();
    std::vector<GLubyte> highlightData(totalVertices, 0);
    glBindVertexArray(_vao);
    glBindBuffer(GL_ARRAY_BUFFER, _vboMode);
    glBufferData(GL_ARRAY_BUFFER, highlightData.size() * sizeof(GLubyte), highlightData.data(), GL_STATIC_DRAW);   


    // Method 2: clear highlight data - using the highlight ebo
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _highlightedLineConnections);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    update();
}

void EmbeddingLinesWidget::setColor(const QColor& color) {
    _color = color;

    // TODO: set the color for the point renderer

    update();
}

void EmbeddingLinesWidget::setColor(int r, int g, int b) 
{
    _color = QColor(r, g, b);
    update();
}

void EmbeddingLinesWidget::setAlpha(float alpha) {
    _alpha = alpha;

    update();
}

void EmbeddingLinesWidget::setHighlights(const std::vector<int>& indices, bool highlightSource)
{
    _highlightIndices = indices;
    _highlightSource = highlightSource;

    //// Method 1: highlight per vertex using the mode buffer
    //size_t totalVertices = _embedding_src.size() + _embedding_dst.size();
    //std::vector<GLubyte> highlightData(totalVertices, 0);

    //if (highlightSource) {
    //    // indices refer to _embedding_src
    //    for (int srcIdx : indices) {
    //        if (srcIdx >= 0 && srcIdx < (int)_embedding_src.size()) {
    //            highlightData[srcIdx] = 1;
    //        }
    //    }
    //}
    //else {
    //    // indices refer to _embedding_dst
    //    size_t offset = _embedding_src.size();
    //    for (int dstIdx : indices) {
    //        if (dstIdx >= 0 && dstIdx < (int)_embedding_dst.size()) {
    //            highlightData[offset + dstIdx] = 1; 
    //        }
    //    }
    //}

    //// upload the highlight data to the GPU
    //glBindVertexArray(_vao);
    //glBindBuffer(GL_ARRAY_BUFFER, _vboMode);
    //glBufferData(GL_ARRAY_BUFFER, highlightData.size() * sizeof(GLubyte), highlightData.data(), GL_STATIC_DRAW);


    // Method 2: highlight per line using the highlighted ebo
    std::unordered_set<int> highlightIndicesSet(_highlightIndices.begin(), _highlightIndices.end());

    size_t srcCount = _embedding_src.size();
    size_t dstCount = _embedding_dst.size();
    size_t totalVertices = srcCount + dstCount;

    std::vector<GLuint> highlightedIndices;
    highlightedIndices.reserve(_lines.size() * 2); // worst case: all lines

    for (auto& line : _lines) {
        GLuint srcIdx = line.first;
        GLuint dstIdx = (GLuint)(srcCount + line.second); // offset for dst

        if ((highlightSource && highlightIndicesSet.count(line.first)) || (!highlightSource && highlightIndicesSet.count(line.second))) 
        { 
            // this line has at least one highlighted endpoint
            highlightedIndices.push_back(srcIdx);
            highlightedIndices.push_back(dstIdx);
        }
    }
    // store the count,needed for rendering
    _highlightedLineCount = (GLsizei)highlightedIndices.size();

    // upload the highlight subset EBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _highlightedLineConnections);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, highlightedIndices.size() * sizeof(GLuint), highlightedIndices.data(), GL_STATIC_DRAW);


    glBindBuffer(GL_ARRAY_BUFFER, 0);

    update();
}

void EmbeddingLinesWidget::onWidgetInitialized()
{
    // Initialize renderers
    _pointRenderer.init();

    // Set a default color map for both renderers
    _pointRenderer.setScalarEffect(PointEffect::None);
    _pointRenderer.setPointSize(5); // TODO: automatically set a proper point size
    _pointRenderer.setSelectionOutlineColor(Vector3f(1, 0, 0));

    glClearColor(255 / 255.0f, 255 / 255.0f, 255 / 255.0f, 1);

    _initialized = true;

    emit initialized();

    qDebug() << "GL initialized";

    // Enable point smoothing
    glEnable(GL_POINT_SMOOTH);

    // Initialize shaders
    if (!_shader.loadShaderFromFile(":dual_view/shaders/EmbeddingLines.vert", ":dual_view/shaders/EmbeddingLines.frag", ":dual_view/shaders/EmbeddingLines.geom")) {
        qCritical() << "Failed to load shaders";
        throw std::runtime_error("Failed to load shaders");
    }
    else {
        qDebug() << "Shaders loaded successfully";
    }

    /*auto m_debugLogger = new QOpenGLDebugLogger(context());
    if (m_debugLogger->initialize()) {
        qDebug() << "GL_DEBUG Debug Logger" << m_debugLogger << "\n";
        connect(m_debugLogger, &QOpenGLDebugLogger::messageLogged, this, [](QOpenGLDebugMessage message) -> void {
            qDebug() << message;
        });

        m_debugLogger->startLogging();
    }*/

    glEnable(GL_MULTISAMPLE);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);


    // for lines
    glGenVertexArrays(1, &_vao);
    glBindVertexArray(_vao);

    // vbo for unique vertices positions in lines
    glGenBuffers(1, &_vboPositions);
    glBindBuffer(GL_ARRAY_BUFFER, _vboPositions);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(mv::Vector2f), (void*)0);
    glEnableVertexAttribArray(0);

    // ebo for line connections
    glGenBuffers(1, &_lineConnections);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _lineConnections);

    // ebo for highlighted line connections
    glGenBuffers(1, &_highlightedLineConnections);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _highlightedLineConnections);

    // vbo for mode (background or foreground color)
    glGenBuffers(1, &_vboMode);
    glBindBuffer(GL_ARRAY_BUFFER, _vboMode);
    glVertexAttribIPointer(1, 1, GL_UNSIGNED_BYTE, 0, (void*)0);
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    // multisampled fbo
    glGenFramebuffers(1, &_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, _fbo);

    // anti-aliasing: multi-sample texture
    glGenTextures(1, &_textureMultiSample);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, _textureMultiSample);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA32F, width(), height(), GL_TRUE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, _textureMultiSample, 0);

    // resolve fbo
    glGenFramebuffers(1, &_resolveFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, _resolveFbo);

    glGenTextures(1, &_resolveTexture);
    glBindTexture(GL_TEXTURE_2D, _resolveTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width(), height(), 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _resolveTexture, 0);
    

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

}

void EmbeddingLinesWidget::onWidgetRendered()
{
    if (!_initialized) {
        throw std::logic_error("OpenGL context must be initialized");
    }

    //qDebug() << "EmbeddingLinesWidget::onWidgetRendered";

    try
    {
        QPainter painter;

        // Begin mixed OpenGL/native painting
        if (!painter.begin(this))
            throw std::runtime_error("Unable to begin painting");

        // Draw layers with OpenGL
        painter.beginNativePainting();
        {
            // Bind the framebuffer
            glBindFramebuffer(GL_FRAMEBUFFER, _fbo);         

            glViewport(0, 0, width(), height());

            glClear(GL_COLOR_BUFFER_BIT);

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            _pointRenderer.render();

            _shader.bind();

            // set the projection matrix
            QMatrix3x3 projection = _pointRenderer.getProjectionMatrix().normalMatrix();
            _shader.uniformMatrix3f("projection", projection.data());
            
            // pass foreground and background colors as uniforms
            _shader.uniform4f("backgroundColor", _color.redF(), _color.greenF(), _color.blueF(), _alpha);
            _shader.uniform4f("foregroundColor", 252.0f / 255.0f, 102.0f / 255.0f, 0.0f / 255.0f, 0.3f); // Orange for highlights, not used anymore

            glBindVertexArray(_vao);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _lineConnections);
            glDrawElements(GL_LINES, static_cast<GLsizei>(2 * _lines.size()), GL_UNSIGNED_INT, 0);

            // FIXME: whether to keep the current shader with foreground and background colors, and mode buffer
            _shader.uniform4f("backgroundColor", 252.0f / 255.0f, 102.0f / 255.0f, 0.0f / 255.0f, 0.07f); // TEMP: set color and alpha for highlighted lines
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _highlightedLineConnections);
            glDrawElements(GL_LINES, _highlightedLineCount, GL_UNSIGNED_INT, 0);

            glBindVertexArray(0);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            // resolve multisampled fbo to resolve fbo
            glBindFramebuffer(GL_READ_FRAMEBUFFER, _fbo);         // Source: multisampled FBO
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _resolveFbo); // Destination: resolve FBO
            glBlitFramebuffer(0, 0, width(), height(), 0, 0, width(), height(), GL_COLOR_BUFFER_BIT, GL_LINEAR);

            // display
            glBindFramebuffer(GL_READ_FRAMEBUFFER, _resolveFbo);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, defaultFramebufferObject());
            glBlitFramebuffer(0, 0, width(), height(), 0, 0, width(), height(), GL_COLOR_BUFFER_BIT, GL_LINEAR);
            
            //qDebug() << "OnWidgetRendererd w " << width() << " h " << height();

            _shader.release();

            //qDebug() << "PaintGL";

        }
        painter.endNativePainting();

        // Draw the selection overlay if the selection tool is enabled
        if (_pixelSelectionTool.isEnabled()) {
            const auto areaPixmap = _pixelSelectionTool.getAreaPixmap();
            const auto shapePixmap = _pixelSelectionTool.getShapePixmap();

            painter.drawPixmap(rect(), areaPixmap);
            painter.drawPixmap(rect(), shapePixmap);
        }

        // new code in scatterplotwidget - TODO: looks same as above?
        /*painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

        QImage pixelSelectionToolsImage(size(), QImage::Format_ARGB32);

        pixelSelectionToolsImage.fill(Qt::transparent);

        paintPixelSelectionToolNative(_pixelSelectionTool, pixelSelectionToolsImage, painter);

        painter.drawImage(0, 0, pixelSelectionToolsImage);*/

        painter.end();
    }

    catch (std::exception& e)
    {
        exceptionMessageBox("Rendering failed", e);
    }
    catch (...) {
        exceptionMessageBox("Rendering failed");
    }
}

void EmbeddingLinesWidget::onWidgetResized(int w, int h)
{
    _pointRenderer.resize(QSize(w, h));

    //qDebug() << "resized to: " << w << "x" << h;

    // fix resize issue

    glDeleteFramebuffers(1, &_fbo);
    glDeleteTextures(1, &_textureMultiSample);
    glDeleteFramebuffers(1, &_resolveFbo);
    glDeleteTextures(1, &_resolveTexture);

    glGenFramebuffers(1, &_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, _fbo);

    glGenTextures(1, &_textureMultiSample);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, _textureMultiSample);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA32F, w, h, GL_TRUE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, _textureMultiSample, 0);

    glGenFramebuffers(1, &_resolveFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, _resolveFbo);

    glGenTextures(1, &_resolveTexture);
    glBindTexture(GL_TEXTURE_2D, _resolveTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _resolveTexture, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void EmbeddingLinesWidget::onWidgetCleanup()
{
    _pointRenderer.destroy();
}

PixelSelectionTool& EmbeddingLinesWidget::getPixelSelectionTool()
{
    return _pixelSelectionTool;
}

void EmbeddingLinesWidget::paintPixelSelectionToolNative(PixelSelectionTool& pixelSelectionTool, QImage& image, QPainter& painter) const
{
    if (!pixelSelectionTool.isEnabled())
        return;

    QPainter pixelSelectionToolImagePainter(&image);

    pixelSelectionToolImagePainter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    pixelSelectionToolImagePainter.drawPixmap(rect(), pixelSelectionTool.getShapePixmap());
    pixelSelectionToolImagePainter.drawPixmap(rect(), pixelSelectionTool.getAreaPixmap());
}
