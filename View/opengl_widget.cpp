#include "opengl_widget.h"
#include "YUV422Frame.h"
#include <QsLog.h>

const char* vertexShade = R"(
        #version 450 core
        layout(location = 0) in vec2 vertexIn;
        void main(){
            gl_Position = vec4(vertexIn, 0.0, 1.0);
        }
)";

const char* fragShade = R"(
        #version 450 core
        layout(location = 0) out vec4 o_Color;
        void main() {
            o_Color = vec4(0.05, 0.03, 0.02, 1.0);
        }
)";

OpenGLWidget::OpenGLWidget(QWidget *parent)
    :QOpenGLWidget(parent)
{

}

OpenGLWidget::~OpenGLWidget()
{
    makeCurrent();
    vbo.destroy();
    delete program;
    doneCurrent();
}

void OpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();

    glClearColor(0.2f, 0.20f, 0.28f, 1.0f);
    GLfloat vertices[] = {
        0.0f, 0.5f,
        -0.5f, -0.5f,
        0.5f, -0.5f
    };

    vbo.create();
    vbo.bind();
    vbo.allocate(vertices, sizeof(vertices));

    program = new QOpenGLShaderProgram(this);
    program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShade);
    program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragShade);
    program->link();
    program->bind();
}

void OpenGLWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void OpenGLWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT);

    program->bind();
    vbo.bind();

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDisableVertexAttribArray(0);

    vbo.release();
    program->release();
}


void OpenGLWidget::showYUV(QSharedPointer<YUV422Frame> frame)
{
    if(frame.isNull()){
        QLOG_ERROR() << "showYUV's frame is nullptr";
    }
    else if(!m_frame.isNull()){
        m_frame.reset();
    }
    m_frame = frame;
    update();
}
