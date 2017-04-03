/********************************************************************************
** Form generated from reading UI file 'MainWindow.ui'
**
** Created by: Qt User Interface Compiler version 5.6.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindowClass
{
public:
    QAction *actionExit;
    QAction *actionMove;
    QAction *actionPolygon;
    QAction *actionRectangle;
    QAction *actionNew;
    QAction *actionDelete;
    QAction *actionUndo;
    QAction *actionRedo;
    QAction *actionCopy;
    QAction *actionPaste;
    QAction *actionSelectAll;
    QAction *actionOpen;
    QAction *actionSave;
    QAction *actionLayer1;
    QAction *actionLayer2;
    QAction *actionInitialKinematicDiagram;
    QAction *actionSolveInverse;
    QAction *actionRun;
    QAction *actionStop;
    QAction *actionStepForward;
    QAction *actionStepBackward;
    QAction *actionAdjustSketch;
    QWidget *centralWidget;
    QMenuBar *menuBar;
    QMenu *menuFile;
    QMenu *menuMode;
    QMenu *menuEdit;
    QMenu *menuLayer;
    QMenu *menuTool;
    QToolBar *mainToolBar;
    QStatusBar *statusBar;

    void setupUi(QMainWindow *MainWindowClass)
    {
        if (MainWindowClass->objectName().isEmpty())
            MainWindowClass->setObjectName(QStringLiteral("MainWindowClass"));
        MainWindowClass->resize(800, 853);
        actionExit = new QAction(MainWindowClass);
        actionExit->setObjectName(QStringLiteral("actionExit"));
        actionMove = new QAction(MainWindowClass);
        actionMove->setObjectName(QStringLiteral("actionMove"));
        actionMove->setCheckable(true);
        actionPolygon = new QAction(MainWindowClass);
        actionPolygon->setObjectName(QStringLiteral("actionPolygon"));
        actionPolygon->setCheckable(true);
        actionRectangle = new QAction(MainWindowClass);
        actionRectangle->setObjectName(QStringLiteral("actionRectangle"));
        actionRectangle->setCheckable(true);
        actionNew = new QAction(MainWindowClass);
        actionNew->setObjectName(QStringLiteral("actionNew"));
        actionDelete = new QAction(MainWindowClass);
        actionDelete->setObjectName(QStringLiteral("actionDelete"));
        actionUndo = new QAction(MainWindowClass);
        actionUndo->setObjectName(QStringLiteral("actionUndo"));
        actionRedo = new QAction(MainWindowClass);
        actionRedo->setObjectName(QStringLiteral("actionRedo"));
        actionCopy = new QAction(MainWindowClass);
        actionCopy->setObjectName(QStringLiteral("actionCopy"));
        actionPaste = new QAction(MainWindowClass);
        actionPaste->setObjectName(QStringLiteral("actionPaste"));
        actionSelectAll = new QAction(MainWindowClass);
        actionSelectAll->setObjectName(QStringLiteral("actionSelectAll"));
        actionOpen = new QAction(MainWindowClass);
        actionOpen->setObjectName(QStringLiteral("actionOpen"));
        actionSave = new QAction(MainWindowClass);
        actionSave->setObjectName(QStringLiteral("actionSave"));
        actionLayer1 = new QAction(MainWindowClass);
        actionLayer1->setObjectName(QStringLiteral("actionLayer1"));
        actionLayer1->setCheckable(true);
        actionLayer2 = new QAction(MainWindowClass);
        actionLayer2->setObjectName(QStringLiteral("actionLayer2"));
        actionLayer2->setCheckable(true);
        actionInitialKinematicDiagram = new QAction(MainWindowClass);
        actionInitialKinematicDiagram->setObjectName(QStringLiteral("actionInitialKinematicDiagram"));
        actionSolveInverse = new QAction(MainWindowClass);
        actionSolveInverse->setObjectName(QStringLiteral("actionSolveInverse"));
        actionRun = new QAction(MainWindowClass);
        actionRun->setObjectName(QStringLiteral("actionRun"));
        actionStop = new QAction(MainWindowClass);
        actionStop->setObjectName(QStringLiteral("actionStop"));
        actionStepForward = new QAction(MainWindowClass);
        actionStepForward->setObjectName(QStringLiteral("actionStepForward"));
        actionStepBackward = new QAction(MainWindowClass);
        actionStepBackward->setObjectName(QStringLiteral("actionStepBackward"));
        actionAdjustSketch = new QAction(MainWindowClass);
        actionAdjustSketch->setObjectName(QStringLiteral("actionAdjustSketch"));
        centralWidget = new QWidget(MainWindowClass);
        centralWidget->setObjectName(QStringLiteral("centralWidget"));
        MainWindowClass->setCentralWidget(centralWidget);
        menuBar = new QMenuBar(MainWindowClass);
        menuBar->setObjectName(QStringLiteral("menuBar"));
        menuBar->setGeometry(QRect(0, 0, 800, 21));
        menuFile = new QMenu(menuBar);
        menuFile->setObjectName(QStringLiteral("menuFile"));
        menuMode = new QMenu(menuBar);
        menuMode->setObjectName(QStringLiteral("menuMode"));
        menuEdit = new QMenu(menuBar);
        menuEdit->setObjectName(QStringLiteral("menuEdit"));
        menuLayer = new QMenu(menuBar);
        menuLayer->setObjectName(QStringLiteral("menuLayer"));
        menuTool = new QMenu(menuBar);
        menuTool->setObjectName(QStringLiteral("menuTool"));
        MainWindowClass->setMenuBar(menuBar);
        mainToolBar = new QToolBar(MainWindowClass);
        mainToolBar->setObjectName(QStringLiteral("mainToolBar"));
        MainWindowClass->addToolBar(Qt::TopToolBarArea, mainToolBar);
        statusBar = new QStatusBar(MainWindowClass);
        statusBar->setObjectName(QStringLiteral("statusBar"));
        MainWindowClass->setStatusBar(statusBar);

        menuBar->addAction(menuFile->menuAction());
        menuBar->addAction(menuEdit->menuAction());
        menuBar->addAction(menuMode->menuAction());
        menuBar->addAction(menuLayer->menuAction());
        menuBar->addAction(menuTool->menuAction());
        menuFile->addAction(actionNew);
        menuFile->addAction(actionOpen);
        menuFile->addAction(actionSave);
        menuFile->addSeparator();
        menuFile->addAction(actionExit);
        menuMode->addAction(actionMove);
        menuMode->addAction(actionRectangle);
        menuMode->addAction(actionPolygon);
        menuEdit->addAction(actionUndo);
        menuEdit->addAction(actionRedo);
        menuEdit->addSeparator();
        menuEdit->addAction(actionCopy);
        menuEdit->addAction(actionPaste);
        menuEdit->addSeparator();
        menuEdit->addAction(actionDelete);
        menuEdit->addSeparator();
        menuEdit->addAction(actionSelectAll);
        menuLayer->addAction(actionLayer1);
        menuLayer->addAction(actionLayer2);
        menuTool->addAction(actionAdjustSketch);
        menuTool->addAction(actionInitialKinematicDiagram);
        menuTool->addAction(actionSolveInverse);
        menuTool->addSeparator();
        menuTool->addAction(actionRun);
        menuTool->addAction(actionStop);
        menuTool->addSeparator();
        menuTool->addAction(actionStepForward);
        menuTool->addAction(actionStepBackward);

        retranslateUi(MainWindowClass);

        QMetaObject::connectSlotsByName(MainWindowClass);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindowClass)
    {
        MainWindowClass->setWindowTitle(QApplication::translate("MainWindowClass", "Kinematic Design", 0));
        actionExit->setText(QApplication::translate("MainWindowClass", "Exit", 0));
        actionMove->setText(QApplication::translate("MainWindowClass", "Move", 0));
        actionPolygon->setText(QApplication::translate("MainWindowClass", "Polygon", 0));
        actionRectangle->setText(QApplication::translate("MainWindowClass", "Rectangle", 0));
        actionNew->setText(QApplication::translate("MainWindowClass", "New", 0));
        actionNew->setShortcut(QApplication::translate("MainWindowClass", "Ctrl+N", 0));
        actionDelete->setText(QApplication::translate("MainWindowClass", "Delete", 0));
        actionDelete->setShortcut(QApplication::translate("MainWindowClass", "Del", 0));
        actionUndo->setText(QApplication::translate("MainWindowClass", "Undo", 0));
        actionUndo->setShortcut(QApplication::translate("MainWindowClass", "Ctrl+Z", 0));
        actionRedo->setText(QApplication::translate("MainWindowClass", "Redo", 0));
        actionRedo->setShortcut(QApplication::translate("MainWindowClass", "Ctrl+Y", 0));
        actionCopy->setText(QApplication::translate("MainWindowClass", "Copy", 0));
        actionCopy->setShortcut(QApplication::translate("MainWindowClass", "Ctrl+C", 0));
        actionPaste->setText(QApplication::translate("MainWindowClass", "Paste", 0));
        actionPaste->setShortcut(QApplication::translate("MainWindowClass", "Ctrl+V", 0));
        actionSelectAll->setText(QApplication::translate("MainWindowClass", "Select All", 0));
        actionSelectAll->setShortcut(QApplication::translate("MainWindowClass", "Ctrl+A", 0));
        actionOpen->setText(QApplication::translate("MainWindowClass", "Open", 0));
        actionOpen->setShortcut(QApplication::translate("MainWindowClass", "Ctrl+O", 0));
        actionSave->setText(QApplication::translate("MainWindowClass", "Save", 0));
        actionSave->setShortcut(QApplication::translate("MainWindowClass", "Ctrl+S", 0));
        actionLayer1->setText(QApplication::translate("MainWindowClass", "Layer 1", 0));
        actionLayer2->setText(QApplication::translate("MainWindowClass", "Layer 2", 0));
        actionInitialKinematicDiagram->setText(QApplication::translate("MainWindowClass", "Initial Kinematic Diagram", 0));
        actionSolveInverse->setText(QApplication::translate("MainWindowClass", "Solve Inverse", 0));
        actionRun->setText(QApplication::translate("MainWindowClass", "Run", 0));
        actionStop->setText(QApplication::translate("MainWindowClass", "Stop", 0));
        actionStepForward->setText(QApplication::translate("MainWindowClass", "Step Forward", 0));
        actionStepForward->setShortcut(QApplication::translate("MainWindowClass", "Right", 0));
        actionStepBackward->setText(QApplication::translate("MainWindowClass", "Step Backward", 0));
        actionStepBackward->setShortcut(QApplication::translate("MainWindowClass", "Left", 0));
        actionAdjustSketch->setText(QApplication::translate("MainWindowClass", "Adjust Sketch", 0));
        menuFile->setTitle(QApplication::translate("MainWindowClass", "File", 0));
        menuMode->setTitle(QApplication::translate("MainWindowClass", "Mode", 0));
        menuEdit->setTitle(QApplication::translate("MainWindowClass", "Edit", 0));
        menuLayer->setTitle(QApplication::translate("MainWindowClass", "Layer", 0));
        menuTool->setTitle(QApplication::translate("MainWindowClass", "Tool", 0));
    } // retranslateUi

};

namespace Ui {
    class MainWindowClass: public Ui_MainWindowClass {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
