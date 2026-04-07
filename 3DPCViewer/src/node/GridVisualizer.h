#ifndef GRID_H
#define GRID_H


#include <osg/Group>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/StateSet>
#include <osg/Program>
#include <osg/Uniform>
#include <osg/BlendFunc>
#include <osg/Depth>

class Grid {
public:
	Grid(float size = 1000.0f) {
        _gridGeode = new osg::Geode();

        // 创建渲染容器
        osg::ref_ptr<osg::Geometry> quad = new osg::Geometry();
        osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();

        // 设置范围
        float s = size;
        vertices->push_back(osg::Vec3(-s, -s, 0.0f));
        vertices->push_back(osg::Vec3(s, -s, 0.0f));
        vertices->push_back(osg::Vec3(s, s, 0.0f));
        vertices->push_back(osg::Vec3(-s, s, 0.0f));

        quad->setVertexArray(vertices);
        quad->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::QUADS, 0, 4));

        // 配置着色器
        osg::ref_ptr<osg::Program> program = new osg::Program;
        program->addShader(new osg::Shader(osg::Shader::VERTEX, vertSource));
        program->addShader(new osg::Shader(osg::Shader::FRAGMENT, fragSource));

        osg::StateSet* ss = quad->getOrCreateStateSet();
        ss->setAttributeAndModes(program, osg::StateAttribute::ON);

        // 配置渲染状态：开启混合（透明），关闭光照，深度写入微调
        ss->setAttributeAndModes(new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
        ss->setMode(GL_LIGHTING, osg::StateAttribute::OFF);

        // 设置渲染顺序
        ss->setRenderBinDetails(-1, "RenderBin");

        // 设置参数
        ss->addUniform(new osg::Uniform("gridSpacing", 1.0f));      // 格子大小
        ss->addUniform(new osg::Uniform("lineWidth", 0.1f));      // 线条粗细
        ss->addUniform(new osg::Uniform("majorGridStep", 10.0f));  // 主网格步长（每10格加深）
        ss->addUniform(new osg::Uniform("gridColor", osg::Vec4(0.5f, 0.5f, 0.5f, 0.6f)));
        ss->addUniform(new osg::Uniform("axisXColor", osg::Vec4(0.8f, 0.1f, 0.1f, 1.0f))); // X轴红色
        ss->addUniform(new osg::Uniform("axisYColor", osg::Vec4(0.1f, 0.8f, 0.1f, 1.0f))); // Y轴绿色

        _gridGeode->addDrawable(quad);
	}
	~Grid() {}
	osg::ref_ptr<osg::Geode> getGridNode() const { return _gridGeode; }
private:
	osg::ref_ptr<osg::Geode> _gridGeode;
    // Vertex Shader: 计算世界坐标并传递给片元
    const char* vertSource = R"(
        varying vec3 vWorldPos;
        void main() {
            vWorldPos = gl_Vertex.xyz;
            gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
        }
    )";

    // Fragment Shader
    const char* fragSource = R"(
        varying vec3 vWorldPos;
        uniform float gridSpacing;
        uniform float lineWidth;
        uniform float majorGridStep;
        uniform vec4 gridColor;
        uniform vec4 axisXColor;
        uniform vec4 axisYColor;

        float getGrid(float pos, float spacing, float width) {
            float dist = abs(fract(pos / spacing - 0.5) - 0.5) / (fwidth(pos) / spacing);
            return 1.0 - smoothstep(0.0, width, dist);
        }

        void main() {
            float x = vWorldPos.x;
            float y = vWorldPos.y;

            // 基础网格
            float lineX = getGrid(x, gridSpacing, lineWidth);
            float lineY = getGrid(y, gridSpacing, lineWidth);
            
            // 主网格（深色线）
            float majorX = getGrid(x, gridSpacing * majorGridStep, lineWidth * 0.8);
            float majorY = getGrid(y, gridSpacing * majorGridStep, lineWidth * 0.8);

            float grid = max(lineX, lineY);
            float major = max(majorX, majorY);

            // 最终颜色混合
            vec4 color = gridColor;
            if (major > 0.1) color *= 1.5; // 加深主网格

            // 坐标轴突出显示 (X轴是 y=0 的线，Y轴是 x=0 的线)
            if (abs(y) < gridSpacing * 0.1) color = mix(color, axisXColor, getGrid(y, 10000.0, 0.02));
            if (abs(x) < gridSpacing * 0.1) color = mix(color, axisYColor, getGrid(x, 10000.0, 0.02));

            // 计算到中心的距离，实现边缘淡出（Infinite的感觉）
            float dist = length(vWorldPos.xy);
            float fade = 1.0 - smoothstep(0.0, 1000.0, dist); // 1000是淡出半径

            gl_FragColor = vec4(color.rgb, max(grid, major) * color.a * fade);
        }
    )";
};

#endif // GRID_H