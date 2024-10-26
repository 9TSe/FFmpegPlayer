#ifndef YUV422FRAME_H
#define YUV422FRAME_H

#include <memory>

class YUV422Frame{
public:
    YUV422Frame(uint8_t *buffer, uint32_t pixelW, uint32_t pixelH)
        :m_buffer(nullptr)
    {
        create(buffer, pixelW, pixelH);
    }

    ~YUV422Frame()
    {
        if(m_buffer != nullptr){
            free(m_buffer);
        }
    }

    inline uint8_t *getBufferY() const {return m_buffer;}
    inline uint8_t *getBufferU() const {return m_buffer + m_pixelH * m_pixelW;}
    inline uint8_t *getBufferV() const {return m_buffer + m_pixelH * m_pixelW * 3 / 2;}
    inline uint32_t getPixelW() const {return m_pixelW;}
    inline uint32_t getPixelH() const {return m_pixelH;}


private:
    void create(uint8_t *buffer, uint32_t pixelW, uint32_t pixelH)
    {
        m_pixelW = pixelW;
        m_pixelH = pixelH;
        int size = pixelH * pixelW;
        if(!m_buffer){
            m_buffer = (uint8_t*)malloc(size * 2);
            memcpy(m_buffer, buffer, size);
            memcpy(m_buffer + size, buffer + size, size >> 1);
            memcpy(m_buffer + size * 3 / 2, buffer + size * 3 / 2 , size >> 1);
        }
    }

private:
    uint8_t *m_buffer;
    uint32_t m_pixelW;
    uint32_t m_pixelH;
};

#endif // YUV422FRAME_H
