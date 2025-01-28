#pragma once

#include <widgets/OpenGLWidget.h>

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QMatrix4x4>
#include <QColor>
#include <vector>
#include <utility>

#include <QPainter>

#include <renderers/PointRenderer.h>

#include "src/MyShader.h" // a customised version of the shader class that is used in the ManiVault core

#include "graphics/Vector2f.h"
#include "graphics/Vector3f.h"
#include "graphics/Bounds.h"
#include "graphics/Texture.h"

#include <util/PixelSelectionTool.h>

using namespace mv;
using namespace mv::util;
using namespace mv::gui;

class EmbeddingLinesWidget : public OpenGLWidget
{
    Q_OBJECT

public:
    EmbeddingLinesWidget();

    void setData(const std::vector<mv::Vector2f>& embedding_src, const std::vector<mv::Vector2f>& embedding_dst);
    void setLines(const std::vector<std::pair<uint32_t, uint32_t>>& lines);
    void setColor(const QColor& color);
    void setColor(int r, int g, int b);
    void setAlpha(float alpha);
    void setHighlights(const std::vector<int>& indices, bool highlightSource);

    void setPointColorA(const std::vector<Vector3f>& pointColors);
    void setPointColorB(const std::vector<Vector3f>& pointColors);   

    /** Get reference to the pixel selection tool */
    PixelSelectionTool& getPixelSelectionTool();

    mv::Bounds getBounds() const {
        return _bounds;
    }

signals:
    void initialized();

protected:
    void onWidgetInitialized()          override;
    void onWidgetResized(int w, int h)  override;
    void onWidgetRendered()             override;
    void onWidgetCleanup()              override;

    void paintPixelSelectionToolNative(PixelSelectionTool& pixelSelectionTool, QImage& image, QPainter& painter) const;


private:
    /*const scalar_type* _embedding_src; 
    const scalar_type* _embedding_dst;*/

    std::vector<mv::Vector2f> _embedding_src; //TODO: change to pointer
    std::vector<mv::Vector2f> _embedding_dst;
    std::vector<std::pair<uint32_t, uint32_t>> _lines;
    std::vector<float> _alpha_per_line;

    QColor _color;
    float _alpha;
    bool _initialized;

    // Highlighting
    std::vector<int> _highlightIndices; // Indices of points to highlight
    bool _highlightSource; // Flag to indicate whether to highlight source or destination

    GLuint _vao;

    GLuint _vboPositions; // point positions

    GLuint _lineConnections; // ebo for line connections (all line connections)

    // test for drawing highlighted lines separately
    GLuint _highlightedLineConnections; // ebo for highlighted line connections
    GLsizei _highlightedLineCount; // number of highlighted lines

    GLuint _vboMode; // background or foreground color

    GLuint _fbo; // multi-sample framebuffer
    GLuint _textureMultiSample; // multi-sample texture 
    GLuint _resolveFbo;
    GLuint _resolveTexture;

    MyShaderProgram _shader;

    PixelSelectionTool      _pixelSelectionTool;        /** 2D pixel selection tool */

    // Point rendering
    PointRenderer           _pointRenderer;     /* ManiVault OpenGL point renderer implementation */
    Bounds                  _bounds;            /* Min and max point coordinates for camera placement */
    std::vector<Vector3f>   _colors;            /* Color of points - here we use a constant color for simplicity */
};
