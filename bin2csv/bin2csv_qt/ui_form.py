# -*- coding: utf-8 -*-

################################################################################
## Form generated from reading UI file 'form.ui'
##
## Created by: Qt User Interface Compiler version 6.9.0
##
## WARNING! All changes made in this file will be lost when recompiling UI file!
################################################################################

from PySide6.QtCore import (QCoreApplication, QDate, QDateTime, QLocale,
    QMetaObject, QObject, QPoint, QRect,
    QSize, QTime, QUrl, Qt)
from PySide6.QtGui import (QBrush, QColor, QConicalGradient, QCursor,
    QFont, QFontDatabase, QGradient, QIcon,
    QImage, QKeySequence, QLinearGradient, QPainter,
    QPalette, QPixmap, QRadialGradient, QTransform)
from PySide6.QtWidgets import (QApplication, QLabel, QLineEdit, QPushButton,
    QSizePolicy, QSpinBox, QTextEdit, QWidget)

class Ui_bin2csv_qt(object):
    def setupUi(self, bin2csv_qt):
        if not bin2csv_qt.objectName():
            bin2csv_qt.setObjectName(u"bin2csv_qt")
        bin2csv_qt.resize(748, 401)
        sizePolicy = QSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(bin2csv_qt.sizePolicy().hasHeightForWidth())
        bin2csv_qt.setSizePolicy(sizePolicy)
        bin2csv_qt.setMinimumSize(QSize(748, 401))
        bin2csv_qt.setMaximumSize(QSize(748, 401))
        self.label = QLabel(bin2csv_qt)
        self.label.setObjectName(u"label")
        self.label.setGeometry(QRect(10, 10, 110, 20))
        self.lineEditInputDir = QLineEdit(bin2csv_qt)
        self.lineEditInputDir.setObjectName(u"lineEditInputDir")
        self.lineEditInputDir.setGeometry(QRect(6, 30, 621, 25))
        self.pushButtonInputDirBrowse = QPushButton(bin2csv_qt)
        self.pushButtonInputDirBrowse.setObjectName(u"pushButtonInputDirBrowse")
        self.pushButtonInputDirBrowse.setGeometry(QRect(640, 30, 88, 25))
        self.pushButtonOutputFileBrowse = QPushButton(bin2csv_qt)
        self.pushButtonOutputFileBrowse.setObjectName(u"pushButtonOutputFileBrowse")
        self.pushButtonOutputFileBrowse.setGeometry(QRect(640, 100, 88, 25))
        self.lineEditOutputFile = QLineEdit(bin2csv_qt)
        self.lineEditOutputFile.setObjectName(u"lineEditOutputFile")
        self.lineEditOutputFile.setGeometry(QRect(6, 100, 621, 25))
        self.label_2 = QLabel(bin2csv_qt)
        self.label_2.setObjectName(u"label_2")
        self.label_2.setGeometry(QRect(10, 80, 111, 20))
        self.pushButtonConvert = QPushButton(bin2csv_qt)
        self.pushButtonConvert.setObjectName(u"pushButtonConvert")
        self.pushButtonConvert.setGeometry(QRect(507, 160, 221, 31))
        self.textEditConversionProgress = QTextEdit(bin2csv_qt)
        self.textEditConversionProgress.setObjectName(u"textEditConversionProgress")
        self.textEditConversionProgress.setGeometry(QRect(8, 219, 721, 161))
        self.label_3 = QLabel(bin2csv_qt)
        self.label_3.setObjectName(u"label_3")
        self.label_3.setGeometry(QRect(10, 150, 91, 17))
        self.spinBoxSampleRate = QSpinBox(bin2csv_qt)
        self.spinBoxSampleRate.setObjectName(u"spinBoxSampleRate")
        self.spinBoxSampleRate.setGeometry(QRect(6, 170, 81, 26))
        self.spinBoxSampleRate.setValue(60)

        self.retranslateUi(bin2csv_qt)

        QMetaObject.connectSlotsByName(bin2csv_qt)
    # setupUi

    def retranslateUi(self, bin2csv_qt):
        bin2csv_qt.setWindowTitle(QCoreApplication.translate("bin2csv_qt", u"bin2csv", None))
        self.label.setText(QCoreApplication.translate("bin2csv_qt", u"Input directory", None))
        self.pushButtonInputDirBrowse.setText(QCoreApplication.translate("bin2csv_qt", u"Browse", None))
        self.pushButtonOutputFileBrowse.setText(QCoreApplication.translate("bin2csv_qt", u"Browse", None))
        self.label_2.setText(QCoreApplication.translate("bin2csv_qt", u"Output file", None))
        self.pushButtonConvert.setText(QCoreApplication.translate("bin2csv_qt", u"Convert", None))
        self.label_3.setText(QCoreApplication.translate("bin2csv_qt", u"Sample rate", None))
    # retranslateUi

