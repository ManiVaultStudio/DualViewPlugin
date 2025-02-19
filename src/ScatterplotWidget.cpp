#include "ScatterplotWidget.h"

#include <CoreInterface.h>

#include <util/Exception.h>

#include <vector>

#include <QDebug>
#include <QGuiApplication>
#include <QMatrix4x4>
#include <QOpenGLFramebufferObject>
#include <QPainter>
#include <QSize>
#include <QWheelEvent>
#include <QWindow>
#include <QRectF>

#include <math.h>

#include <ViewPlugin.h>

using namespace mv;

namespace
{
    Bounds getDataBounds(const std::vector<Vector2f>& points)
    {
        Bounds bounds = Bounds::Max;

        for (const Vector2f& point : points)
        {
            bounds.setLeft(std::min(point.x, bounds.getLeft()));
            bounds.setRight(std::max(point.x, bounds.getRight()));
            bounds.setBottom(std::min(point.y, bounds.getBottom()));
            bounds.setTop(std::max(point.y, bounds.getTop()));
        }

        return bounds;
    }

    void translateBounds(Bounds& b, float x, float y)
    {
        b.setLeft(b.getLeft() + x);
        b.setRight(b.getRight() + x);
        b.setBottom(b.getBottom() + y);
        b.setTop(b.getTop() + y);
    }
}

ScatterplotWidget::ScatterplotWidget(mv::plugin::ViewPlugin* parentPlugin) :
    QOpenGLWidget(),
    _pointRenderer(),
    _densityRenderer(DensityRenderer::RenderMode::DENSITY),
    _isInitialized(false),
    _renderMode(SCATTERPLOT),
    _backgroundColor(255, 255, 255, 255),
    _coloringMode(ColoringMode::Constant),
    _widgetSizeInfo(),
    _dataRectangleAction(this, "Data rectangle"),
    _navigationAction(this, "Navigation"),
    _colorMapImage(),
    _pixelSelectionTool(this),
    _samplerPixelSelectionTool(this),
    _pixelRatio(1.0),
    _mousePositions(),
    _isNavigating(false),
    _weightDensity(false),
    _parentPlugin(parentPlugin)
{
    setContextMenuPolicy(Qt::CustomContextMenu);
    setAcceptDrops(true);
    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);
    grabGesture(Qt::PinchGesture);
    //setAttribute(Qt::WA_TranslucentBackground);
    installEventFilter(this);

    _navigationAction.initialize(this);

    _pixelSelectionTool.setEnabled(true);
    _pixelSelectionTool.setMainColor(QColor(Qt::black));
    _pixelSelectionTool.setFixedBrushRadiusModifier(Qt::AltModifier);

    _samplerPixelSelectionTool.setEnabled(true);
    _samplerPixelSelectionTool.setMainColor(QColor(Qt::black));
    _samplerPixelSelectionTool.setFixedBrushRadiusModifier(Qt::AltModifier);

    connect(&_pixelSelectionTool, &PixelSelectionTool::shapeChanged, [this]() {
        if (isInitialized())
            update();
    });

    connect(&_samplerPixelSelectionTool, &PixelSelectionTool::shapeChanged, [this]() {
        if (isInitialized())
            update();
    });

    QSurfaceFormat surfaceFormat;
    surfaceFormat.setRenderableType(QSurfaceFormat::OpenGL);
    surfaceFormat.setVersion(3, 3); 
    surfaceFormat.setProfile(QSurfaceFormat::CoreProfile);
    surfaceFormat.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    surfaceFormat.setSamples(16);
    surfaceFormat.setStencilBufferSize(8);

#ifdef _DEBUG
    surfaceFormat.setOption(QSurfaceFormat::DebugContext);
#endif

    setFormat(surfaceFormat);
    
    // Call updatePixelRatio when the window is moved between hi and low dpi screens
    // e.g., from a laptop display to a projector
    // Wait with the connection until we are sure that the window is created
    connect(this, &ScatterplotWidget::created, this, [this](){
        [[maybe_unused]] auto windowID = this->window()->winId(); // This is needed to produce a valid windowHandle on some systems

        QWindow* winHandle = windowHandle();

        // On some systems we might need to use a different windowHandle
        if(!winHandle)
        {
            const QWidget* nativeParent = nativeParentWidget();
            winHandle = nativeParent->windowHandle();
        }
        
        if(winHandle == nullptr)
        {
            qDebug() << "ScatterplotWidget: Not connecting updatePixelRatio - could not get window handle";
            return;
        }

        QObject::connect(winHandle, &QWindow::screenChanged, this, &ScatterplotWidget::updatePixelRatio, Qt::UniqueConnection);
    });

    connect(&_navigationAction.getZoomRectangleAction(), &DecimalRectangleAction::rectangleChanged, this, [this]() -> void {
        auto& zoomRectangleAction = _navigationAction.getZoomRectangleAction();

        const auto zoomBounds = zoomRectangleAction.getBounds();

        _pointRenderer.setViewBounds(zoomBounds);
        _densityRenderer.setBounds(zoomBounds);

        _navigationAction.getZoomDataExtentsAction().setEnabled(zoomBounds != _dataRectangleAction.getBounds());

        update();
    });
}

bool ScatterplotWidget::event(QEvent* event)
{
    if (!event)
        return QOpenGLWidget::event(event);

    auto setIsNavigating = [this](bool isNavigating) -> void {
        _isNavigating = isNavigating;
        _pixelSelectionTool.setEnabled(!isNavigating);
        if (isNavigating) {
            _samplerPixelSelectionTool.setEnabled(false);
        }
        else if (_parentPlugin) { // reset to UI-setting
            _samplerPixelSelectionTool.setEnabled(_parentPlugin->getSamplerAction().getEnabledAction().isChecked());
        }

        };

    // Set navigation flag on Alt press/release
    if (event->type() == QEvent::KeyRelease) {
        if (const auto* keyEvent = static_cast<QKeyEvent*>(event)) {
            if (keyEvent->key() == Qt::Key_Alt) {
                setIsNavigating(false);
            }
        }

    }
    else if (event->type() == QEvent::KeyPress) {
        if (const auto* keyEvent = static_cast<QKeyEvent*>(event)) {
            if (keyEvent->key() == Qt::Key_Alt) {
                setIsNavigating(true);
            }
        }

    }

    // Interactions when Alt is pressed
    if (isInitialized() && QGuiApplication::keyboardModifiers() == Qt::AltModifier) {

        switch (event->type())
        {
            case QEvent::Wheel:
            {
                // Scroll to zoom
                if (auto* wheelEvent = static_cast<QWheelEvent*>(event))
                    zoomAround(wheelEvent->position().toPoint(), wheelEvent->angleDelta().x() / 1200.f);

                break;
            }

            case QEvent::MouseButtonPress:
            {
                if (const auto* mouseEvent = static_cast<QMouseEvent*>(event))
                {
                    if(mouseEvent->button() == Qt::MiddleButton)
                        resetView();

                    // Navigation
                    if (mouseEvent->buttons() == Qt::LeftButton)
                    {
                        setIsNavigating(true);
                        setCursor(Qt::ClosedHandCursor);
                        _mousePositions << mouseEvent->pos();
                        update();
                    }
                }

                break;
            }

            case QEvent::MouseButtonRelease:
            {
                setIsNavigating(false);
                setCursor(Qt::ArrowCursor);
                _mousePositions.clear();
                update();

                break;
            }

            case QEvent::MouseMove:
            {
                if (const auto* mouseEvent = static_cast<QMouseEvent*>(event))
                {
                    _mousePositions << mouseEvent->pos();

                    if (mouseEvent->buttons() == Qt::LeftButton && _mousePositions.size() >= 2) 
                    {
                        const auto& previousMousePosition   = _mousePositions[_mousePositions.size() - 2];
                        const auto& currentMousePosition    = _mousePositions[_mousePositions.size() - 1];
                        const auto panVector                = currentMousePosition - previousMousePosition;

                        panBy(panVector);
                    }
                }

                break;
            }

        }
    
    }

    return QOpenGLWidget::event(event);
}

void ScatterplotWidget::resetView()
{
    _navigationAction.getZoomRectangleAction().setBounds(_dataRectangleAction.getBounds());
}

void ScatterplotWidget::panBy(const QPointF& to)
{
    auto& zoomRectangleAction = _navigationAction.getZoomRectangleAction();

    const auto moveBy = QPointF(to.x() / _widgetSizeInfo.width * zoomRectangleAction.getWidth() * _widgetSizeInfo.ratioWidth * -1.f,
                                to.y() / _widgetSizeInfo.height * zoomRectangleAction.getHeight() * _widgetSizeInfo.ratioHeight);

    zoomRectangleAction.translateBy({ moveBy.x(), moveBy.y() });

    update();
}

void ScatterplotWidget::zoomAround(const QPointF& at, float factor)
{
    auto& zoomRectangleAction = _navigationAction.getZoomRectangleAction();

    // the widget might have a different aspect ratio than the square opengl viewport
    const auto offsetBounds = QPointF(zoomRectangleAction.getWidth()  * (0.5f * (1 - _widgetSizeInfo.ratioWidth)),
                                      zoomRectangleAction.getHeight() * (0.5f * (1 - _widgetSizeInfo.ratioHeight)) * -1.f);

    const auto originBounds = QPointF(zoomRectangleAction.getLeft(), zoomRectangleAction.getTop());

    // translate mouse point in widget to mouse point in bounds coordinates
    const auto atTransformed = QPointF(at.x() / _widgetSizeInfo.width * zoomRectangleAction.getWidth() * _widgetSizeInfo.ratioWidth,
                                       at.y() / _widgetSizeInfo.height * zoomRectangleAction.getHeight() * _widgetSizeInfo.ratioHeight * -1.f);

    const auto atInBounds = originBounds + offsetBounds + atTransformed;

    // ensure mouse position is the same after zooming
    const auto currentBoundCenter = zoomRectangleAction.getCenter();

    float moveMouseX = (atInBounds.x() - currentBoundCenter.first) * factor;
    float moveMouseY = (atInBounds.y() - currentBoundCenter.second) * factor;

    // zoom and move view
    zoomRectangleAction.translateBy({ moveMouseX, moveMouseY });
    zoomRectangleAction.expandBy(-1.f * factor);

    update();
}

bool ScatterplotWidget::isInitialized() const
{
    return _isInitialized;
}

ScatterplotWidget::RenderMode ScatterplotWidget::getRenderMode() const
{
    return _renderMode;
}

void ScatterplotWidget::setRenderMode(const RenderMode& renderMode)
{
    if (renderMode == _renderMode)
        return;

    _renderMode = renderMode;

    emit renderModeChanged(_renderMode);

    switch (_renderMode)
    {
        case ScatterplotWidget::SCATTERPLOT:
            break;
        
        case ScatterplotWidget::DENSITY:
            computeDensity();
            break;

        case ScatterplotWidget::LANDSCAPE:
            computeDensity();
            break;

        default:
            break;
    }

    update();
}

ScatterplotWidget::ColoringMode ScatterplotWidget::getColoringMode() const
{
    return _coloringMode;
}

void ScatterplotWidget::setColoringMode(const ColoringMode& coloringMode)
{
    if (coloringMode == _coloringMode)
        return;

    _coloringMode = coloringMode;

    emit coloringModeChanged(_coloringMode);
}

PixelSelectionTool& ScatterplotWidget::getPixelSelectionTool()
{
    return _pixelSelectionTool;
}

PixelSelectionTool& ScatterplotWidget::getSamplerPixelSelectionTool()
{
    return _samplerPixelSelectionTool;
}

void ScatterplotWidget::computeDensity()
{
    emit densityComputationStarted();

    _densityRenderer.computeDensity();

    emit densityComputationEnded();

    update();
}

// Positions need to be passed as a pointer as we need to store them locally in order
// to be able to find the subset of data that's part of a selection. If passed
// by reference then we can upload the data to the GPU, but not store it in the widget.
void ScatterplotWidget::setData(const std::vector<Vector2f>* points)
{
    auto dataBounds = getDataBounds(*points);

    // pass un-adjusted data bounds to renderer for 2D colormapping
    _pointRenderer.setDataBounds(dataBounds);

    // Adjust data points for projection matrix creation (add a little white space around data)
    dataBounds.ensureMinimumSize(1e-07f, 1e-07f);
    dataBounds.makeSquare();
    dataBounds.expand(0.1f);

    const auto shouldSetBounds = (mv::projects().isOpeningProject() || mv::projects().isImportingProject()) ? false : !_navigationAction.getFreezeZoomAction().isChecked();

    if (shouldSetBounds)
        _pointRenderer.setViewBounds(dataBounds);

    _densityRenderer.setBounds(dataBounds);

    _dataRectangleAction.setBounds(dataBounds);

    if (shouldSetBounds)
        _navigationAction.getZoomRectangleAction().setBounds(dataBounds);

    _pointRenderer.setData(*points);
    _densityRenderer.setData(points);

    switch (_renderMode)
    {
        case ScatterplotWidget::SCATTERPLOT:
            break;
        
        case ScatterplotWidget::DENSITY:
        case ScatterplotWidget::LANDSCAPE:
        {
            _densityRenderer.computeDensity();
            break;
        }

        default:
            break;
    }
   // _pointRenderer.setSelectionOutlineColor(Vector3f(1, 0, 0));

    update();
}

QColor ScatterplotWidget::getBackgroundColor() const
{
    return _backgroundColor;
}

void ScatterplotWidget::setBackgroundColor(QColor color)
{
    _backgroundColor = color;

    update();
}

void ScatterplotWidget::setHighlights(const std::vector<char>& highlights, const std::int32_t& numSelectedPoints)
{
    _pointRenderer.setHighlights(highlights, numSelectedPoints);

    update();
}

void ScatterplotWidget::setScalars(const std::vector<float>& scalars)
{
    _pointRenderer.setColorChannelScalars(scalars);
    
    update();
}

void ScatterplotWidget::setColors(const std::vector<Vector3f>& colors)
{
    _pointRenderer.setColors(colors);
    _pointRenderer.setScalarEffect(None);

    update();
}

void ScatterplotWidget::setPointSizeScalars(const std::vector<float>& pointSizeScalars)
{
    if (pointSizeScalars.empty())
        return;

    _pointRenderer.setSizeChannelScalars(pointSizeScalars);
    _pointRenderer.setPointSize(*std::max_element(pointSizeScalars.begin(), pointSizeScalars.end()));

    update();
}

void ScatterplotWidget::setPointOpacityScalars(const std::vector<float>& pointOpacityScalars)
{
    _pointRenderer.setOpacityChannelScalars(pointOpacityScalars);

    update();
}

void ScatterplotWidget::setPointScaling(mv::gui::PointScaling scalingMode)
{
    _pointRenderer.setPointScaling(scalingMode);

    update();
}

void ScatterplotWidget::setScalarEffect(PointEffect effect)
{
    _pointRenderer.setScalarEffect(effect);

    update();
}

void ScatterplotWidget::setSigma(const float sigma)
{
    _densityRenderer.setSigma(sigma);

    update();
}

void ScatterplotWidget::setWeightDensity(bool useWeights) 
{ 
    _weightDensity = useWeights; 

    const std::vector<float>* weights = nullptr;

    if (_weightDensity)
        weights = &_pointRenderer.getGpuPoints().getSizeScalars();

    _densityRenderer.setWeights(weights);
}

mv::Vector3f ScatterplotWidget::getColorMapRange() const
{
    switch (_renderMode) {
        case SCATTERPLOT:
            return _pointRenderer.getColorMapRange();

        case LANDSCAPE:
            return _densityRenderer.getColorMapRange();

        default:
            break;
    }
    
    return Vector3f();
}

void ScatterplotWidget::setColorMapRange(const float& min, const float& max)
{
    switch (_renderMode) {
        case SCATTERPLOT:
        {
            _pointRenderer.setColorMapRange(min, max);
            break;
        }

        case LANDSCAPE:
        {
            _densityRenderer.setColorMapRange(min, max);
            break;
        }

        default:
            break;
    }

    update();
}

void ScatterplotWidget::showHighlights(bool show)
{
    _pointRenderer.setSelectionOutlineScale(show ? 0.5f : 0);
    update();
}

void ScatterplotWidget::createScreenshot(std::int32_t width, std::int32_t height, const QString& fileName, const QColor& backgroundColor)
{
    // Exit if the viewer is not initialized
    if (!_isInitialized)
        return;

    // Exit prematurely if the file name is invalid
    if (fileName.isEmpty())
        return;

    makeCurrent();

    try {

        // Use custom FBO format
        QOpenGLFramebufferObjectFormat fboFormat;
        
        fboFormat.setTextureTarget(GL_TEXTURE_2D);
        fboFormat.setInternalTextureFormat(GL_RGB);

        QOpenGLFramebufferObject fbo(width, height, fboFormat);

        // Bind the FBO and render into it when successfully bound
        if (fbo.bind()) {

            // Clear the widget to the background color
            glClearColor(backgroundColor.redF(), backgroundColor.greenF(), backgroundColor.blueF(), backgroundColor.alphaF());
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Reset the blending function
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            // Resize OpenGL to intended screenshot size
            resizeGL(width, height);

            switch (_renderMode)
            {
                case SCATTERPLOT:
                {
                    _pointRenderer.setPointScaling(Relative);
                    _pointRenderer.render();
                    _pointRenderer.setPointScaling(Absolute);

                    break;
                }

                case DENSITY:
                case LANDSCAPE:
                    _densityRenderer.setRenderMode(_renderMode == DENSITY ? DensityRenderer::DENSITY : DensityRenderer::LANDSCAPE);
                    _densityRenderer.render();
                    break;
            }

            // Save FBO image to disk
            //fbo.toImage(false, QImage::Format_RGB32).convertToFormat(QImage::Format_RGB32).save(fileName);
            //fbo.toImage(false, QImage::Format_ARGB32).save(fileName);

            QImage fboImage(fbo.toImage());
            QImage image(fboImage.constBits(), fboImage.width(), fboImage.height(), QImage::Format_ARGB32);

            image.save(fileName);

            // Resize OpenGL back to original OpenGL widget size
            resizeGL(this->width(), this->height());

            fbo.release();
        }
    }
    catch (std::exception& e)
    {
        exceptionMessageBox("Rendering failed", e);
    }
    catch (...) {
        exceptionMessageBox("Rendering failed");
    }
}

PointSelectionDisplayMode ScatterplotWidget::getSelectionDisplayMode() const
{
    return _pointRenderer.getSelectionDisplayMode();
}

void ScatterplotWidget::setSelectionDisplayMode(PointSelectionDisplayMode selectionDisplayMode)
{
    _pointRenderer.setSelectionDisplayMode(selectionDisplayMode);

    update();
}

QColor ScatterplotWidget::getSelectionOutlineColor() const
{
    QColor haloColor;

    haloColor.setRedF(_pointRenderer.getSelectionOutlineColor().x);
    haloColor.setGreenF(_pointRenderer.getSelectionOutlineColor().y);
    haloColor.setBlueF(_pointRenderer.getSelectionOutlineColor().z);

    return haloColor;
}

void ScatterplotWidget::setSelectionOutlineColor(const QColor& selectionOutlineColor)
{
    _pointRenderer.setSelectionOutlineColor(Vector3f(selectionOutlineColor.redF(), selectionOutlineColor.greenF(), selectionOutlineColor.blueF()));

   update();
}

bool ScatterplotWidget::getSelectionOutlineOverrideColor() const
{
    return _pointRenderer.getSelectionOutlineOverrideColor();
}

void ScatterplotWidget::setSelectionOutlineOverrideColor(bool selectionOutlineOverrideColor)
{
    _pointRenderer.setSelectionOutlineOverrideColor(selectionOutlineOverrideColor);

    update();
}

float ScatterplotWidget::getSelectionOutlineScale() const
{
    return _pointRenderer.getSelectionOutlineScale();
}

void ScatterplotWidget::setSelectionOutlineScale(float selectionOutlineScale)
{
    _pointRenderer.setSelectionOutlineScale(selectionOutlineScale);

    update();
}

float ScatterplotWidget::getSelectionOutlineOpacity() const
{
    return _pointRenderer.getSelectionOutlineOpacity();
}

void ScatterplotWidget::setSelectionOutlineOpacity(float selectionOutlineOpacity)
{
    _pointRenderer.setSelectionOutlineOpacity(selectionOutlineOpacity);

    update();
}

bool ScatterplotWidget::getSelectionOutlineHaloEnabled() const
{
    return _pointRenderer.getSelectionHaloEnabled();
}

void ScatterplotWidget::setSelectionOutlineHaloEnabled(bool selectionOutlineHaloEnabled)
{
    _pointRenderer.setSelectionHaloEnabled(selectionOutlineHaloEnabled);

    update();
}

void ScatterplotWidget::setRandomizedDepthEnabled(bool randomizedDepth)
{
    _pointRenderer.setRandomizedDepthEnabled(randomizedDepth);

    update();
}

bool ScatterplotWidget::getRandomizedDepthEnabled() const
{
    return _pointRenderer.getRandomizedDepthEnabled();
}

void ScatterplotWidget::initializeGL()
{
    initializeOpenGLFunctions();

#ifdef SCATTER_PLOT_WIDGET_VERBOSE
    qDebug() << "Initializing scatterplot widget with context: " << context();

    std::string versionString = std::string((const char*) glGetString(GL_VERSION));

    qDebug() << versionString.c_str();
#endif

    connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, &ScatterplotWidget::cleanup);

    // Initialize renderers
    _pointRenderer.init();
    _densityRenderer.init();

    // Set a default color map for both renderers
    _pointRenderer.setScalarEffect(PointEffect::Color);

    _pointRenderer.setPointScaling(Absolute);
    _pointRenderer.setSelectionOutlineColor(Vector3f(1, 0, 0));

    // OpenGL is initialized
    _isInitialized = true;

    // Initialize the point and density renderer with a color map
    setColorMap(_colorMapImage);

    emit initialized();
}

void ScatterplotWidget::resizeGL(int w, int h)
{
    _widgetSizeInfo.width       = static_cast<float>(w);
    _widgetSizeInfo.height      = static_cast<float>(h);
    _widgetSizeInfo.minWH       = _widgetSizeInfo.width < _widgetSizeInfo.height ? _widgetSizeInfo.width : _widgetSizeInfo.height;
    _widgetSizeInfo.ratioWidth  = _widgetSizeInfo.width / _widgetSizeInfo.minWH;
    _widgetSizeInfo.ratioHeight = _widgetSizeInfo.height / _widgetSizeInfo.minWH;

    // we need this here as we do not have the screen yet to get the actual devicePixelRatio when the view is created
    _pixelRatio = devicePixelRatio();

    // Pixel ratio tells us how many pixels map to a point
    // That is needed as macOS calculates in points and we do in pixels
    // On macOS high dpi displays pixel ration is 2
    w *= _pixelRatio;
    h *= _pixelRatio;

    _pointRenderer.resize(QSize(w, h));
    _densityRenderer.resize(QSize(w, h));
}

void ScatterplotWidget::paintGL()
{
    try {
        QPainter painter;

        // Begin mixed OpenGL/native painting
        if (!painter.begin(this))
            throw std::runtime_error("Unable to begin painting");

        // Draw layers with OpenGL
        painter.beginNativePainting();
        {
            // Bind the framebuffer belonging to the widget
            // glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());

            // Clear the widget to the background color
            glClearColor(_backgroundColor.redF(), _backgroundColor.greenF(), _backgroundColor.blueF(), _backgroundColor.alphaF());
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Reset the blending function
            glEnable(GL_BLEND);

            if (getRandomizedDepthEnabled())
                glEnable(GL_DEPTH_TEST);

            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
               
            switch (_renderMode)
            {
                case SCATTERPLOT:
                    _pointRenderer.render();
                    break;

                case DENSITY:
                case LANDSCAPE:
                    _densityRenderer.setRenderMode(_renderMode == DENSITY ? DensityRenderer::DENSITY : DensityRenderer::LANDSCAPE);
                    _densityRenderer.render();
                    break;
            }
                
        }
        painter.endNativePainting();

        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

        QImage pixelSelectionToolsImage(size(), QImage::Format_ARGB32);

        pixelSelectionToolsImage.fill(Qt::transparent);

        paintPixelSelectionToolNative(_pixelSelectionTool, pixelSelectionToolsImage, painter);
        paintPixelSelectionToolNative(_samplerPixelSelectionTool, pixelSelectionToolsImage, painter);

        painter.drawImage(0, 0, pixelSelectionToolsImage);

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

void ScatterplotWidget::paintPixelSelectionToolNative(PixelSelectionTool& pixelSelectionTool, QImage& image, QPainter& painter) const
{
    if (!pixelSelectionTool.isEnabled())
        return;

    QPainter pixelSelectionToolImagePainter(&image);

    pixelSelectionToolImagePainter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    pixelSelectionToolImagePainter.drawPixmap(rect(), pixelSelectionTool.getShapePixmap());
    pixelSelectionToolImagePainter.drawPixmap(rect(), pixelSelectionTool.getAreaPixmap());
}

void ScatterplotWidget::cleanup()
{
    qDebug() << "Deleting scatterplot widget, performing clean up...";
    _isInitialized = false;

    makeCurrent();
    _pointRenderer.destroy();
    _densityRenderer.destroy();
}

void ScatterplotWidget::setColorMap(const QImage& colorMapImage)
{
    _colorMapImage = colorMapImage;

    // Do not update color maps of the renderers when OpenGL is not initialized
    if (!_isInitialized)
        return;

    // Apply color maps to renderers
    _pointRenderer.setColormap(_colorMapImage);
    _densityRenderer.setColormap(_colorMapImage);

    // Render
    update();
}

void ScatterplotWidget::updatePixelRatio()
{
    float pixelRatio = devicePixelRatio();
    
#ifdef SCATTER_PLOT_WIDGET_VERBOSE
    qDebug() << "Window moved to screen " << window()->screen() << ".";
    qDebug() << "Pixelratio before was " << _pixelRatio << ". New pixelratio is: " << pixelRatio << ".";
#endif // SCATTER_PLOT_WIDGET_VERBOSE
    
    // we only update if the ratio actually changed
    if( _pixelRatio != pixelRatio )
    {
        _pixelRatio = pixelRatio;
        resizeGL(width(), height());
        update();
    }
}

ScatterplotWidget::~ScatterplotWidget()
{
    disconnect(QOpenGLWidget::context(), &QOpenGLContext::aboutToBeDestroyed, this, &ScatterplotWidget::cleanup);
    cleanup();
}
