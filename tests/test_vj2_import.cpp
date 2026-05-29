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

QTEST_MAIN(TestVj2Import)
#include "test_vj2_import.moc"
