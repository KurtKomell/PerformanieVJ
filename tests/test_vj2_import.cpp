#include "core/Project.h"
#include "core/Vj2Importer.h"

#include <QTemporaryFile>
#include <QtTest>

using pvj::core::Project;
using pvj::core::Vj2Importer;

class TestVj2Import : public QObject
{
    Q_OBJECT

private slots:
    void cellPropertyMappings_importTransparencyFadeSpeed();
    void cellPropertyMappings_nestedValueElements();
    void cellKeyboardTriggers_nestedInBankSet();
    void matrixDimensions_fromDataAttributes();
    void matrixDimensions_inferredFromCellCount();
    void bankNames_fromNameAttribute();
    void mergeIntoBank_fromSourceOffset();
};

void TestVj2Import::cellPropertyMappings_importTransparencyFadeSpeed()
{
    static const char kXml[] = R"(<?xml version="1.0"?>
<GrandVJ version="2.7">
  <PROJECT>
    <DATA MATRIXWIDTH="12" MATRIXHEIGHT="4"/>
    <BANKSET type="0">
      <BANK index="0" name="Bank 1">
        <CELLS>
          <CELL index="0">
            <PROPERTYMAPPINGLIST>
              <PROPERTYMAPPING TARGET="TRSP" CHANNEL="midi:0:1:cc:7"/>
              <PROPERTYMAPPING TARGET="FADE" CHANNEL="midi:0:1:cc:8"/>
              <PROPERTYMAPPING TARGET="BRGT" CHANNEL="midi:0:1:cc:9"/>
              <PROPERTYMAPPING TARGET="MSPD" CHANNEL="midi:0:1:cc:10"/>
              <PROPERTYMAPPING TARGET="FXP1" CHANNEL="midi:0:1:cc:11"/>
            </PROPERTYMAPPINGLIST>
          </CELL>
        </CELLS>
      </BANK>
    </BANKSET>
  </PROJECT>
</GrandVJ>
)";

    QTemporaryFile tmp;
    QVERIFY(tmp.open());
    tmp.write(kXml);
    tmp.close();

    Project project;
    const Vj2Importer::Result res = Vj2Importer::importFile(project, tmp.fileName());
    QVERIFY2(res.ok, qPrintable(res.errorMessage));
    QVERIFY(!project.bankSets.isEmpty());
    QVERIFY(!project.bankSets[0].banks.isEmpty());
    const auto& cell = project.bankSets[0].banks[0].cells[0];
    QCOMPARE(cell.propertyMappings.size(), 4);

    QCOMPARE(cell.propertyMappings[0].property, QStringLiteral("transparency"));
    QCOMPARE(cell.propertyMappings[0].input, pvj::core::InputType::MidiCC);
    QCOMPARE(cell.propertyMappings[0].channel, 0); // GrandVJ ch 1 -> internal 0
    QCOMPARE(cell.propertyMappings[0].number, 7);

    QCOMPARE(cell.propertyMappings[1].property, QStringLiteral("fade"));
    QCOMPARE(cell.propertyMappings[2].property, QStringLiteral("pictureBrightness"));
    QCOMPARE(cell.propertyMappings[3].property, QStringLiteral("movieSpeed"));
    QVERIFY(cell.propertyMappings[3].minValue < 0.0);
    QVERIFY(cell.propertyMappings[3].maxValue > 1.0);
}

void TestVj2Import::cellPropertyMappings_nestedValueElements()
{
    static const char kXml[] = R"(<?xml version="1.0"?>
<GrandVJ version="2.7">
  <PROJECT>
    <DATA MATRIXWIDTH="12" MATRIXHEIGHT="4"/>
    <BANKSET type="0">
      <BANK index="0" name="Bank 1">
        <CELLS>
          <CELL index="0">
            <PROPERTYMAPPINGLIST>
              <PROPERTYMAPPING>
                <TARGET VALUE="TRSP"/>
                <CHANNEL VALUE="midi:0:1:cc:7"/>
              </PROPERTYMAPPING>
            </PROPERTYMAPPINGLIST>
          </CELL>
        </CELLS>
      </BANK>
    </BANKSET>
  </PROJECT>
</GrandVJ>
)";

    QTemporaryFile tmp;
    QVERIFY(tmp.open());
    tmp.write(kXml);
    tmp.close();

    Project project;
    const Vj2Importer::Result res = Vj2Importer::importFile(project, tmp.fileName());
    QVERIFY2(res.ok, qPrintable(res.errorMessage));
    const auto& cell = project.bankSets[0].banks[0].cells[0];
    QCOMPARE(cell.propertyMappings.size(), 1);
    QCOMPARE(cell.propertyMappings[0].property, QStringLiteral("transparency"));
    QCOMPARE(cell.propertyMappings[0].channel, 0);
    QCOMPARE(cell.propertyMappings[0].number, 7);
}

void TestVj2Import::cellKeyboardTriggers_nestedInBankSet()
{
    static const char kXml[] = R"xml(<?xml version="1.0"?>
<PROJECT VERSION="2">
  <DATA MATRIXWIDTH="12" MATRIXHEIGHT="5"/>
  <BANKSET TYPE="0" ACTIVE="0">
    <BANK INDEX="0" NAME="Bank 1">
      <CELLS>
        <CELL INDEX="0"/>
        <CELL INDEX="1"/>
      </CELLS>
    </BANK>
    <TRIGGERMAPPINGS TYPE="MIDI">
      <CELLTRIGGERMAPPING MAPPING="midi:all:0:note:60" CELLID="1"/>
    </TRIGGERMAPPINGS>
    <TRIGGERMAPPINGS TYPE="KEYBOARD">
      <CELLTRIGGERMAPPING MAPPING="keyboard:0:F1" CELLID="0"/>
      <CELLTRIGGERMAPPING MAPPING="keyboard:0:A" CELLID="1"/>
      <CELLTRIGGERMAPPING MAPPING="keyboard:0:#1" CELLID="2"/>
      <CELLTRIGGERMAPPING MAPPING="keyboard:0:#2" CELLID="3"/>
      <CELLTRIGGERMAPPING MAPPING="keyboard:0:SFT" CELLID="4"/>
      <CELLTRIGGERMAPPING MAPPING="keyboard:0:\" CELLID="5"/>
      <CELLTRIGGERMAPPING MAPPING="keyboard:0:#" CELLID="6"/>
      <CELLTRIGGERMAPPING MAPPING="keyboard:0:&apos;" CELLID="7"/>
    </TRIGGERMAPPINGS>
  </BANKSET>
  <BANKSET TYPE="1" ACTIVE="0">
    <TRIGGERMAPPINGS TYPE="KEYBOARD">
      <CELLTRIGGERMAPPING MAPPING="keyboard:0:Z" CELLID="0"/>
    </TRIGGERMAPPINGS>
  </BANKSET>
</PROJECT>
)xml";

    QTemporaryFile tmp;
    QVERIFY(tmp.open());
    tmp.write(kXml);
    tmp.close();

    Project project;
    const Vj2Importer::Result res = Vj2Importer::importFile(project, tmp.fileName());
    QVERIFY2(res.ok, qPrintable(res.errorMessage));
    QCOMPARE(project.bankSets.size(), 1); // TypeB dropped

    QCOMPARE(project.keyboardTriggerForCell(0, 0, 0), QStringLiteral("F1"));
    QCOMPARE(project.keyboardTriggerForCell(0, 0, 1), QStringLiteral("A"));
    QCOMPARE(project.keyboardTriggerForCell(0, 0, 2), QStringLiteral("Shift"));
    QCOMPARE(project.keyboardTriggerForCell(0, 0, 3), QStringLiteral("\\"));
    QCOMPARE(project.keyboardTriggerForCell(0, 0, 4), QStringLiteral("Shift"));
    QCOMPARE(project.keyboardTriggerForCell(0, 0, 5), QStringLiteral("\\"));
    QCOMPARE(project.keyboardTriggerForCell(0, 0, 6), QStringLiteral("#"));
    QCOMPARE(project.keyboardTriggerForCell(0, 0, 7), QStringLiteral("'"));

    int midiCh = -1;
    int midiNote = -1;
    project.midiCellTriggerForCell(0, 0, 1, &midiCh, &midiNote);
    QCOMPARE(midiCh, 0);
    QCOMPARE(midiNote, 60);
}

void TestVj2Import::matrixDimensions_fromDataAttributes()
{
    static const char kXml[] = R"(<?xml version="1.0"?>
<GrandVJ version="2.7.3">
  <PROJECT>
    <DATA MODE="VJ" MATRIXWIDTH="12" MATRIXHEIGHT="6"/>
    <BANKSET TYPE="0">
      <BANK INDEX="0" NAME="Bank 1">
        <CELLS>
          <CELL/>
          <CELL/>
          <CELL/>
        </CELLS>
      </BANK>
    </BANKSET>
  </PROJECT>
</GrandVJ>
)";

    QTemporaryFile tmp;
    QVERIFY(tmp.open());
    tmp.write(kXml);
    tmp.close();

    Project project;
    const Vj2Importer::Result res = Vj2Importer::importFile(project, tmp.fileName());
    QVERIFY2(res.ok, qPrintable(res.errorMessage));
    QCOMPARE(project.settings.matrix.gridCols, 12);
    QCOMPARE(project.settings.matrix.gridRows, 6);
    QCOMPARE(project.bankSets[0].banks[0].cells.size(), 72);
}

void TestVj2Import::matrixDimensions_inferredFromCellCount()
{
    // No DATA matrix size — importer should grow the grid from the cell list (60 → 12×5).
    QByteArray xml = R"(<?xml version="1.0"?>
<PROJECT VERSION="2">
  <BANKSET TYPE="0">
    <BANK INDEX="0" NAME="Bank 1">
      <CELLS>
)";
    for (int i = 0; i < 60; ++i) {
        xml += "        <CELL/>\n";
    }
    xml += R"(      </CELLS>
    </BANK>
  </BANKSET>
</PROJECT>
)";

    QTemporaryFile tmp;
    QVERIFY(tmp.open());
    tmp.write(xml);
    tmp.close();

    Project project;
    const Vj2Importer::Result res = Vj2Importer::importFile(project, tmp.fileName());
    QVERIFY2(res.ok, qPrintable(res.errorMessage));
    QCOMPARE(project.settings.matrix.gridCols, 12);
    QCOMPARE(project.settings.matrix.gridRows, 5);
    QCOMPARE(project.bankSets[0].banks[0].cells.size(), 60);
}

void TestVj2Import::bankNames_fromNameAttribute()
{
    static const char kXml[] = R"(<?xml version="1.0"?>
<PROJECT VERSION="2">
  <DATA MATRIXWIDTH="12" MATRIXHEIGHT="5"/>
  <BANKSET TYPE="0">
    <BANK>
      <CELLS>
        <CELL/>
      </CELLS>
      <NAME name="Babel2"/>
    </BANK>
    <BANK>
      <CELLS>
        <CELL/>
      </CELLS>
      <NAME name="Im Spiegel2"/>
    </BANK>
    <BANK INDEX="2" NAME="AttrName">
      <CELLS>
        <CELL/>
      </CELLS>
    </BANK>
  </BANKSET>
</PROJECT>
)";

    QTemporaryFile tmp;
    QVERIFY(tmp.open());
    tmp.write(kXml);
    tmp.close();

    Project project;
    const Vj2Importer::Result res = Vj2Importer::importFile(project, tmp.fileName());
    QVERIFY2(res.ok, qPrintable(res.errorMessage));
    QVERIFY(project.bankSets[0].banks.size() >= 3);
    QCOMPARE(project.bankSets[0].banks[0].name, QStringLiteral("Babel2"));
    QCOMPARE(project.bankSets[0].banks[1].name, QStringLiteral("Im Spiegel2"));
    QCOMPARE(project.bankSets[0].banks[2].name, QStringLiteral("AttrName"));
}

void TestVj2Import::mergeIntoBank_fromSourceOffset()
{
    static const char kXml[] = R"(<?xml version="1.0"?>
<PROJECT VERSION="2">
  <DATA MATRIXWIDTH="12" MATRIXHEIGHT="4"/>
  <BANKSET TYPE="0">
    <BANK>
      <CELLS><CELL/></CELLS>
      <NAME name="SrcA"/>
    </BANK>
    <BANK>
      <CELLS><CELL/></CELLS>
      <NAME name="SrcB"/>
    </BANK>
    <BANK>
      <CELLS><CELL/></CELLS>
      <NAME name="SrcC"/>
    </BANK>
  </BANKSET>
</PROJECT>
)";

    QTemporaryFile tmp;
    QVERIFY(tmp.open());
    tmp.write(kXml);
    tmp.close();

    Project dest;
    dest.initializeDefault();
    dest.bankSets[0].banks[0].name = QStringLiteral("Keep0");
    dest.bankSets[0].banks[1].name = QStringLiteral("Keep1");
    dest.bankSets[0].banks[2].name = QStringLiteral("Old2");

    // Merge source banks starting at SrcB into destination bank index 2.
    const Vj2Importer::Result res = Vj2Importer::mergeFile(dest, tmp.fileName(), /*dest*/ 2, /*src*/ 1);
    QVERIFY2(res.ok, qPrintable(res.errorMessage));
    QCOMPARE(res.banksMerged, 2);
    QCOMPARE(dest.bankSets[0].banks[0].name, QStringLiteral("Keep0"));
    QCOMPARE(dest.bankSets[0].banks[1].name, QStringLiteral("Keep1"));
    QCOMPARE(dest.bankSets[0].banks[2].name, QStringLiteral("SrcB"));
    QCOMPARE(dest.bankSets[0].banks[3].name, QStringLiteral("SrcC"));
}

QTEST_MAIN(TestVj2Import)
#include "test_vj2_import.moc"
