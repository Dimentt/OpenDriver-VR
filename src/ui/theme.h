#pragma once
#include <QString>

namespace opendriver::ui {

inline QString GetDefaultStylesheet() {
    return R"(
        * { font-family: 'Segoe UI', 'Inter', 'Roboto', sans-serif; }

        QMainWindow, QDialog { background: #0f1117; }

        QTabWidget::pane {
            border: 1px solid #2a2d3a;
            background: #161822;
            border-radius: 8px;
        }
        QTabBar::tab {
            background: #1c1f2e;
            color: #8b8fa3;
            padding: 10px 22px;
            margin-right: 2px;
            border-top-left-radius: 8px;
            border-top-right-radius: 8px;
            font-weight: 600;
            font-size: 12px;
        }
        QTabBar::tab:selected {
            background: #161822;
            color: #e0e4f0;
            border-bottom: 2px solid #6c63ff;
        }
        QTabBar::tab:hover:!selected {
            background: #22253a;
            color: #c0c4d4;
        }

        QGroupBox {
            background: #1c1f2e;
            border: 1px solid #2a2d3a;
            border-radius: 10px;
            margin-top: 14px;
            padding: 18px 14px 14px 14px;
            font-weight: 700;
            font-size: 13px;
            color: #c8cce0;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 4px 12px;
            background: #6c63ff;
            color: white;
            border-radius: 6px;
            font-size: 11px;
        }

        QLabel { color: #b0b4c8; font-size: 12px; }

        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #6c63ff, stop:1 #8b5cf6);
            color: white;
            border: none;
            border-radius: 6px;
            padding: 8px 18px;
            font-weight: 600;
            font-size: 12px;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #7c73ff, stop:1 #9b6cf6);
        }
        QPushButton:pressed { background: #5a52e0; }
        QPushButton:disabled {
            background: #2a2d3a;
            color: #666;
        }
        QPushButton[danger="true"] {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #ef4444, stop:1 #f87171);
        }
        QPushButton[secondary="true"] {
            background: #2a2d3a;
            color: #b0b4c8;
        }
        QPushButton[secondary="true"]:hover { background: #363a4e; }

        QLineEdit {
            background: #1c1f2e;
            border: 1px solid #2a2d3a;
            border-radius: 6px;
            padding: 8px 12px;
            color: #e0e4f0;
            font-size: 12px;
        }
        QLineEdit:focus { border: 1px solid #6c63ff; }

        QTableView, QListView {
            background: #1c1f2e;
            alternate-background-color: #20243a;
            border: 1px solid #2a2d3a;
            border-radius: 8px;
            color: #d0d4e4;
            gridline-color: #2a2d3a;
            font-size: 12px;
            selection-background-color: #3730a3;
            selection-color: white;
        }
        QHeaderView::section {
            background: #22253a;
            color: #8b8fa3;
            border: none;
            border-bottom: 2px solid #6c63ff;
            padding: 8px 6px;
            font-weight: 700;
            font-size: 11px;
            text-transform: uppercase;
        }

        QTextEdit {
            background: #12141e;
            color: #c8ccd8;
            border: 1px solid #2a2d3a;
            border-radius: 8px;
            font-family: 'Cascadia Code', 'Consolas', 'Courier New', monospace;
            font-size: 11px;
            padding: 8px;
        }

        QSpinBox, QDoubleSpinBox, QComboBox {
            background: #1c1f2e;
            border: 1px solid #2a2d3a;
            border-radius: 6px;
            padding: 6px 10px;
            color: #e0e4f0;
            font-size: 12px;
        }
        QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
            border: 1px solid #6c63ff;
        }
        QComboBox::drop-down {
            border: none;
            width: 24px;
        }
        QComboBox QAbstractItemView {
            background: #1c1f2e;
            border: 1px solid #2a2d3a;
            color: #e0e4f0;
            selection-background-color: #3730a3;
        }

        QSlider::groove:horizontal {
            height: 6px;
            background: #2a2d3a;
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            background: #6c63ff;
            width: 16px;
            height: 16px;
            margin: -5px 0;
            border-radius: 8px;
        }
        QSlider::sub-page:horizontal {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #6c63ff, stop:1 #8b5cf6);
            border-radius: 3px;
        }

        QSplitter::handle { background: #2a2d3a; height: 2px; }

        QCheckBox { color: #b0b4c8; spacing: 8px; }
        QCheckBox::indicator {
            width: 16px; height: 16px;
            border-radius: 4px;
            border: 2px solid #3a3d4e;
            background: #1c1f2e;
        }
        QCheckBox::indicator:checked {
            background: #6c63ff;
            border-color: #6c63ff;
        }

        QScrollBar:vertical {
            background: transparent;
            width: 8px;
        }
        QScrollBar::handle:vertical {
            background: #3a3d4e;
            border-radius: 4px;
            min-height: 30px;
        }
        QScrollBar::handle:vertical:hover { background: #5a5d6e; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        
        QProgressBar {
            border: 1px solid #2a2d3a;
            border-radius: 6px;
            background-color: #12141e;
            text-align: center;
            color: #e0e4f0;
            font-weight: bold;
        }
        QProgressBar::chunk {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #6c63ff, stop:1 #8b5cf6);
            border-radius: 5px;
        }
    )";
}

} // namespace opendriver::ui
