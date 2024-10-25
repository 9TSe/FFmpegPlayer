#include "opengl_widget.h"
#include "YUV422Frame.h"
#include <QsLog.h>

#define VERTEXIN 0
#define TEXTUREIN 1

// 应用变换矩阵计算顶点在屏幕上的位置
// 将纹理坐标传递给片段着色器以用于纹理采样。
// gl_Postion 屏幕展现的位置
const char* vertexShade = R"(
        #version 450 core
        layout(location = 0) in vec2 vertexIn;
        layout(location = 1) in vec2 textureIn;
        layout(location = 0) out vec2 textureOut;
        uniform mat4 transform;
        void main(void)
        {
            gl_Position = transform * vec4(vertexIn, 0.0, 1.0);
            textureOut = textureIn;
        }
)";

// R = Y + U + V
// G = 0 - 0.39465U + 2.03211V
// B = 1.13983Y - 0.58060U + 0
const char* fragShade = R"(
        #version 450 core
        layout(location = 0) out vec4 o_Color;
        layout(location = 0) in vec2 textureOut;

        uniform sampler2D tex_y;
        uniform sampler2D tex_u;
        uniform sampler2D tex_v;
        void main(void)
        {
            vec3 yuv;
            vec3 rgb;
            yuv.x = texture2D(tex_y, textureOut).r;
            yuv.y = texture2D(tex_u, textureOut).r - 0.5;
            yuv.z = texture2D(tex_v, textureOut).r - 0.5;
            rgb = mat3(1, 1, 1,
                        0, -0.39465, 2.03211,
                        1.13983, -0.58060, 0) * yuv;
            o_Color = vec4(rgb, 1);
        }
)";


OpenGLWidget::OpenGLWidget(QWidget *parent)
    :QOpenGLWidget(parent),
      m_isDoubleClick(false),
      m_transform(Eigen::Matrix4f::Identity())
{
    connect(&m_timer, &QTimer::timeout,[this](){
        if(this->m_isDoubleClick == false){
            emit this->mouseClicked(); // 超时双击未按下判定为单击
        }
        this->m_isDoubleClick = false;
        this->m_timer.stop();
    });
    m_timer.setInterval(400); // 400ms内判定为双击
}

OpenGLWidget::~OpenGLWidget()
{
    makeCurrent();
    vbo.destroy();
    textureY->destroy();
    textureU->destroy();
    textureV->destroy();
    delete program;
    doneCurrent();
}

void OpenGLWidget::initializeGL() // 类初始上下文时触发
{
    initializeOpenGLFunctions();
    //glClearColor(0.2f, 0.20f, 0.28f, 1.0f);
    glClearColor(0.180, 0.180, 0.212, 0.0);
    // 省去被遮挡物的渲染开销
    glEnable(GL_DEPTH_TEST);
    static const GLfloat vertices[] = {
        // 顶点坐标
        -1.0f, -1.0f,
        -1.0f, +1.0f,
        +1.0f, +1.0f,
        +1.0f, -1.0f,
        // 纹理坐标
        0.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
    };

    vbo.create();
    vbo.bind();
    vbo.allocate(vertices, sizeof(vertices));

    program = new QOpenGLShaderProgram(this);
    program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShade);
    program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragShade);
    // 即顶点着色器in变量的传递
    program->bindAttributeLocation("vertexIn", VERTEXIN);
    program->bindAttributeLocation("textureIn", TEXTUREIN);

    if(!program->link()){
        QLOG_ERROR() << "program link error";
        close(); // 关闭glwidget
    }
    if(!program->bind()){
        QLOG_ERROR() << "program bind error";
        close();
    }

    program->enableAttributeArray(VERTEXIN);
    program->enableAttributeArray(TEXTUREIN);
    program->setAttributeBuffer(VERTEXIN, GL_FLOAT, 0, 2, 2 * sizeof(GLfloat));
    program->setAttributeBuffer(TEXTUREIN, GL_FLOAT, 8 * sizeof(GLfloat), 2, 2 * sizeof(GLfloat));

    posUniformY = program->uniformLocation("tex_y");
    posUniformU = program->uniformLocation("tex_u");
    posUniformV = program->uniformLocation("tex_v");
    posUniformTransform = program->uniformLocation("transform");

    textureY = new QOpenGLTexture(QOpenGLTexture::Target2D);
    textureU = new QOpenGLTexture(QOpenGLTexture::Target2D);
    textureV = new QOpenGLTexture(QOpenGLTexture::Target2D);
    textureY->create();
    textureU->create();
    textureV->create();
    m_idY = textureY->textureId();
    m_idU = textureU->textureId();
    m_idV = textureV->textureId();
}

void OpenGLWidget::showYUV(QSharedPointer<YUV422Frame> frame)
{
    //QLOG_INFO() << "show yuv func";
    if(frame.isNull()){
        QLOG_ERROR() << "showYUV's frame is nullptr";
    }
    else if(!m_frame.isNull()){
        m_frame.reset();
    }
    m_frame = frame;
    update(); // paintGL
}

void OpenGLWidget::paintGL()
{
    if(m_frame.isNull()) return;
    uint32_t videoW = m_frame->getPixelW();
    uint32_t videoH = m_frame->getPixelH();
    m_srcAspectRatio = (float)videoW / (float)videoH;

    if(m_srcAspectRatio <= m_dstAspectRatio){ // 需要变宽
        float dstWMRatio = m_srcAspectRatio * m_dstHeight;
        m_transform(0, 0) = dstWMRatio / m_dstWidth;
        m_transform(1, 1) = 1.f;
    }
    else{
        float dstHRatio = m_dstWidth / m_srcAspectRatio;
        m_transform(0, 0) = 1.f;
        m_transform(1, 1) = dstHRatio / m_dstHeight;
    }

    // 激活纹理单元0(系统内部
    glActiveTexture(GL_TEXTURE0);
    // 绑定y分量纹理id, 到激活纹理单元
    glBindTexture(GL_TEXTURE_2D, m_idY);
    /**
     * @brief glTexImage2D 创建一个二维纹理
     * @param GL_TEXTURE_2D 指定创建的纹理类型
     * @param 0 多级渐远纹理的级别, 基本级别
     * @param GL_RED 纹理的内部格式 红色分量
     * @param w,h 纹理宽高
     * @param 0 历史遗留
     * @param GL_RED 传入的纹理格式
     * @param GL_UNSIGNED_BYTE 数据的类型，这里表示每个颜色分量的数据类型为无符号字节
     */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, videoW, videoH, 0,
                 GL_RED, GL_UNSIGNED_BYTE, m_frame->getBufferY());
    // 纹理的放大(缩小)过滤方法, 线性过滤(根据周围的像素进行线性插值，从而获得更平滑的视觉效果
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    // 纹理在水平(垂直)方向, S轴(T轴)的包裹方式, 夹紧到边缘
    // 纹理坐标超出 [0, 1] 的范围，OpenGL 会使用边缘的颜色而不是重复纹理。
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);


    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_idU);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, videoW >> 1, videoH, 0,
                 GL_RED, GL_UNSIGNED_BYTE, m_frame->getBufferU());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_idV);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, videoW >> 1, videoH, 0,
                 GL_RED, GL_UNSIGNED_BYTE, m_frame->getBufferV());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    /**
     * @brief glUniformMatrix4fv 向当前活动着色器程序的 uniform 变量上传一个 4x4 矩阵
     * @param 1 要传递的矩阵数量
     * @param false 是否要转置矩阵
     * @param data 要上传的矩阵数据的指针
     */
    glUniformMatrix4fv(posUniformTransform, 1, GL_FALSE, m_transform.data());

    //Y 纹理绑定到纹理单元 GL_TEXTURE0
    glUniform1i(posUniformY, 0);
    glUniform1i(posUniformU, 1);
    glUniform1i(posUniformV, 2);
    //使用顶点数组方式绘制图形
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void OpenGLWidget::resizeGL(int w, int h)
{
    if(h == 0) return;
    m_dstWidth = w;
    m_dstHeight = h;
    m_dstAspectRatio = static_cast<float>(w) / static_cast<float>(h);
}

void OpenGLWidget::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    if(!m_timer.isActive()){ // 双击间隔开始计时
        m_timer.start();
    }
}

void OpenGLWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    m_isDoubleClick = true;
    emit mouseDoubleClicked();
}
