#include "pch.h"
\
#include "Splitter.h"

namespace ui {

void Splitter::Attach(HWND parent, HWND left, HWND right) {
    m_parent = parent;
    m_left = left;
    m_right = right;
}

void Splitter::SetRatio(double r) {
    if (r < 0.10) r = 0.10;
    if (r > 0.80) r = 0.80;
    m_ratio = r;
}

RECT Splitter::GripRect(int cx, int cy) const {
    int leftW = (int)(cx * m_ratio);
    RECT rc{ leftW - (m_gripWidth / 2), 0, leftW + (m_gripWidth / 2), cy };
    return rc;
}

void Splitter::Layout(int cx, int cy) {
    int leftW = (int)(cx * m_ratio);
    int grip = m_gripWidth;
    int rightX = leftW + grip;

    MoveWindow(m_left, 0, 0, leftW, cy, TRUE);
    MoveWindow(m_right, rightX, 0, cx - rightX, cy, TRUE);
}

void Splitter::OnSize(int cx, int cy) {
    if (!m_left || !m_right) return;
    Layout(cx, cy);
}

bool Splitter::OnMouseDown(int x, int y) {
    if (!m_parent) return false;
    RECT rc{};
    GetClientRect(m_parent, &rc);
    RECT grip = GripRect(rc.right - rc.left, rc.bottom - rc.top);
    POINT pt{ x, y };
    if (PtInRect(&grip, pt)) {
        m_drag = true;
        m_lastX = x;
        SetCapture(m_parent);
        return true;
    }
    return false;
}

bool Splitter::OnMouseMove(int x, int y) {
    if (!m_parent) return false;
    RECT rc{};
    GetClientRect(m_parent, &rc);
    int cx = rc.right - rc.left;
    int cy = rc.bottom - rc.top;

    if (!m_drag) {
        RECT grip = GripRect(cx, cy);
        POINT pt{ x, y };
        if (PtInRect(&grip, pt)) {
            SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
            return true;
        }
        return false;
    }

    int dx = x - m_lastX;
    m_lastX = x;
    double delta = (cx > 0) ? (double)dx / (double)cx : 0.0;
    SetRatio(m_ratio + delta);
    Layout(cx, cy);
    return true;
}

bool Splitter::OnMouseUp() {
    if (!m_drag) return false;
    m_drag = false;
    ReleaseCapture();
    return true;
}

}