import os
#from bin2csv_qt import bin2csv_qt
import sys

from PySide6.QtWidgets import QApplication, QWidget
from bin2csv_qt.ui_form import Ui_bin2csv_qt
from PySide6.QtWidgets import QFileDialog


readingsPerSecond = 50
binaryFileBlockSize = (6 + 8 * readingsPerSecond)
binaryFilePrefix = "test_"

csvFileDir = ""
csvFileName = "test.csv"

def addDebug(text):
    ui.ui.textEditConversionProgress.append(text)

def convertFiles(dir, output):
    #csvFile = open(csvFileDir + csvFileName, 'w')
    csvFile = open(output, 'w')
    csvFile.write("Date & Time,PPGVal,Xval,Yval,Zval\n")

    it = 0
    while(True):
        fileName = dir + os.sep + binaryFilePrefix + str(it).zfill(2) + ".bin"
        addDebug("Attempting to open " + fileName + "\n")
        if not openFile(fileName, csvFile):
            addDebug("File does not exist, done converting \n")
            break
        it += 1
    pass

def openFile(name, csvFile):
    try:
        file = open(name, 'rb')
    except:
        return False

    try:
        addDebug("Converting data..\n")
        while(True):
#            print("Reading file")
            chunk = file.read(binaryFileBlockSize)

            for it in range(0, readingsPerSecond):
                csvFile.write(str(2000 + chunk[0]) + "/" + str(chunk[1]) + "/" + str(chunk[2]) + " ")
                csvFile.write(str(chunk[3]) + ":" + str(chunk[4]) + ":" + str(chunk[5]))
                for column in range(0,4):
                    readPointer = 6 + it * 8 + column * 2
                    csvFile.write("," + str(chunk[readPointer] + chunk[readPointer + 1] * 256))
                csvFile.write("\n")
            if not chunk:
                break
#        print("Done")
        file.close()
        return chunk
    except:
        return True

class bin2csv_qt(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.ui = Ui_bin2csv_qt()
        self.ui.setupUi(self)

    def browseInputDir(self):
        file = str(QFileDialog.getExistingDirectory(self, "Select Directory"))
        self.ui.lineEditInputDir.setText(file)

    def browseOutputFile(self):
        dialog = QFileDialog(self)
        dialog.setFileMode(QFileDialog.FileMode.AnyFile)
        file = str(dialog.getSaveFileName(self, "Select output file")[0])
        self.ui.lineEditOutputFile.setText(file)

    def convert(self):
        if self.ui.lineEditInputDir.text() == "" or self.ui.lineEditOutputFile.text() == "":
            addDebug("File or directory not specified\n")
            return

        convertFiles(self.ui.lineEditInputDir.text(), self.ui.lineEditOutputFile.text())


if __name__ == "__main__":
    app = QApplication(sys.argv)
    ui = bin2csv_qt()
    ui.show()

    ui.ui.pushButtonInputDirBrowse.pressed.connect(ui.browseInputDir)
    ui.ui.pushButtonOutputFileBrowse.pressed.connect(ui.browseOutputFile)
    ui.ui.pushButtonConvert.pressed.connect(ui.convert)

    sys.exit(app.exec())

    #print("bin2csv")
    #openDir("/home/knas/Desktop/Data/Various Creativity/Chandra_NIH_Project/Test data/Bin multi test")
    pass