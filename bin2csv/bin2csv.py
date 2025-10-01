import os
#from bin2csv_qt import bin2csv_qt
import sys
import wave
import struct

from PySide6.QtWidgets import QApplication, QWidget
from bin2csv_qt.ui_form import Ui_bin2csv_qt
from PySide6.QtWidgets import QFileDialog

readingsPerSecond = 45
binaryFileBlockSize = 6 + (8 * readingsPerSecond)
samplerange = 1024

writewav = False
wav_file: wave

binaryFilePrefix = "test_"

csvFileDir = ""
csvFileName = "test.csv"

def addDebug(text):
    ui.ui.textEditConversionProgress.append(text)

def convertFiles(dir, output):
    #csvFile = open(csvFileDir + csvFileName, 'w')
    csvFile = open(output, 'w')
    csvFile.write("Date & Time,PPGVal,Xval,Yval,Zval\n")

 #   if writewav:
 #       global wav_file
 #       wav_file = wave.open(output + ".wav", mode="wb")
 #       wav_file.setnchannels(1)
 #       wav_file.setsampwidth(2)
 #       wav_file.setframerate(readingsPerSecond)

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
    global wav_file
    wavbuffer = []

    try:
        addDebug("Converting data..\n")
        while(True):
#            print("Reading file")
            chunk = file.read(binaryFileBlockSize)

            for it in range(0, readingsPerSecond):
                csvFile.write(str(2000 + chunk[0]) + "-" + str(chunk[1]).zfill(2) + "-" + str(chunk[2]).zfill(2) + " ")
                csvFile.write(str(chunk[3]).zfill(2) + ":" + str(chunk[4]).zfill(2) + ":" + str(chunk[5]).zfill(2))
#                if writewav:
#                    smp = chunk[6 + it * 8] + chunk[6 + it * 8 + 1] * 256
#                    sample = int((smp - (samplerange / 2)) * (65536 / samplerange))
#                    if (sample < -32767) or (sample > 32767):
#                        pass
#                    wavbuffer.append( sample )
                for column in range(0,4):
                    readPointer = 6 + it * 8 + column * 2
                    csvFile.write("," + str(chunk[readPointer] + chunk[readPointer + 1] * 256))
                csvFile.write("\n")
            if not chunk:
                break
        file.close()
#        if writewav:
#            wav_file.writeframes(struct.pack("<h", wavbuffer))
        return chunk
    except:
        file.close()
#        if writewav:
#            for samples in wavbuffer:
#                wav_file.writeframes( struct.pack("<h", int(samples)) )
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

    def setSampleRate(self):
        global readingsPerSecond
        readingsPerSecond = self.ui.spinBoxSampleRate.value()
        global binaryFileBlockSize
        binaryFileBlockSize= 6 + (8 * readingsPerSecond)

if __name__ == "__main__":
    app = QApplication(sys.argv)
    ui = bin2csv_qt()
    ui.show()

    ui.ui.pushButtonInputDirBrowse.pressed.connect(ui.browseInputDir)
    ui.ui.pushButtonOutputFileBrowse.pressed.connect(ui.browseOutputFile)
    ui.ui.pushButtonConvert.pressed.connect(ui.convert)
    ui.ui.spinBoxSampleRate.setValue(readingsPerSecond)
    ui.ui.spinBoxSampleRate.valueChanged.connect(ui.setSampleRate)

    sys.exit(app.exec())

    #print("bin2csv")
    #openDir("/home/knas/Desktop/Data/Various Creativity/Chandra_NIH_Project/Test data/Bin multi test")
    pass