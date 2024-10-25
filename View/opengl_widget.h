#ifndef OPENGL_WIDGET_H
#define OPENGL_WIDGET_H

#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLTexture>
#include <QTimer>
#include <Eigen/Dense>

class YUV422Frame;

class OpenGLWidget : public QOpenGLWidget, public QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit OpenGLWidget(QWidget* parent = nullptr);
    ~OpenGLWidget();

protected:
    virtual void initializeGL() override;
    virtual void paintGL() override;
    virtual void resizeGL(int w, int h) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;
    virtual void mouseDoubleClickEvent(QMouseEvent *event) override;

public slots:
    void showYUV(QSharedPointer<YUV422Frame> frame);

signals:
    void mouseClicked();
    void mouseDoubleClicked();

private:
    QSharedPointer<YUV422Frame> m_frame;

    // 顶点缓冲区对象
    QOpenGLBuffer vbo;
    // 着色管理器
    QOpenGLShaderProgram *program;
    //yuv分量位置
    GLuint posUniformY;
    GLuint posUniformU;
    GLuint posUniformV;
    // 用于传递变换矩阵
    GLuint posUniformTransform;

    // 纹理
    QOpenGLTexture *textureY = nullptr;
    QOpenGLTexture *textureU = nullptr;
    QOpenGLTexture *textureV = nullptr;

    // 纹理ID, 失败返回0
    GLuint m_idY, m_idU, m_idV;

    QTimer m_timer;

    bool m_isDoubleClick;
    int m_dstWidth, m_dstHeight;
    float m_srcAspectRatio = 1.f;
    float m_dstAspectRatio = 1.f;
    // 00 01 02 03
    // 10 11 12 13
    // 20 21 22 33
    // 30 31 32 33
    // 00: x 方向的缩放因子    10: y 方向的剪切因子  20: z 方向的剪切因子
    // 01: x 方向的剪切      11: y 方向的缩放因子   21: z 方向的缩放因子
    // 02: x 方向的平移      12: y 方向的平移     22: z 方向的深度
    Eigen::Matrix4f m_transform;
};

#endif // OPENGL_WIDGET_H
