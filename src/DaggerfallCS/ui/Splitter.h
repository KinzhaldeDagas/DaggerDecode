#pragma once
#include "../pch.h"

namespace ui {

class Splitter {
public:
    void Attach(HWND parent, HWND left, HWND right);
    void SetRatio(double r);
    void OnSize(int cx, int cy);
    bool OnMouseDown(int x, int y);
    bool OnMouseMove(int x, int y);
    bool OnMouseUp();

private:
    HWND m_parent{};
    HWND m_left{};
    HWND m_right{};
    double m_ratio{ 0.30 };
    int m_gripWidth{ 6 };
    bool m_drag{};
    int m_lastX{};

    RECT GripRect(int cx, int cy) const;
    void Layout(int cx, int cy);
};

}
