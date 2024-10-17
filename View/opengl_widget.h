#ifndef OPENGL_WIDGET_H
#define OPENGL_WIDGET_H

#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>

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

public slots:
    void showYUV(QSharedPointer<YUV422Frame> frame);

private:
    QSharedPointer<YUV422Frame> m_frame;
    QOpenGLBuffer vbo;
    QOpenGLShaderProgram *program;
};

#endif // OPENGL_WIDGET_H
