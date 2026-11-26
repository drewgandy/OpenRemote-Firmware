// Runs WebConfig's log zip code under Node and unpacks the result with Python.
//   node zip.test.cjs
const fs = require("fs"), path = require("path"), assert = require("assert");
const { execFileSync } = require("child_process");
const html = fs.readFileSync(path.join(__dirname, "../../webconfig/WebConfig 3.50.html"), "utf8");
const code = html.slice(html.indexOf("/* LOG ZIP START"), html.indexOf("/* LOG ZIP END */"));
const CRC32_TABLE = Uint32Array.from({ length: 256 }, (_, i) => {
  let c = i;
  for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
  return c >>> 0;
});
const make = () => new Function("CRC32_TABLE", "CompressionStream",
  code + "return {logZipBuild, logZipSelect};");

(async () => {
  const text = (s) => new TextEncoder().encode(s);
  const entries = [
    { name: "debug/20261126-b00001-p001.log", bytes: text("hello log line\n".repeat(5000)) },
    { name: "battery.csv", bytes: new Uint8Array(0) },
    { name: "info.txt", bytes: text("x") },
  ].map((e) => ({ ...e, date: new Date(2026, 10, 26, 14, 30, 10) }));
  // With compression, and as a browser without CompressionStream would.
  for (const compression of [CompressionStream, undefined]) {
    const { logZipBuild } = make()(CRC32_TABLE, compression);
    const zip = Buffer.from(await (await logZipBuild(entries)).arrayBuffer());
    const file = path.join(require("os").tmpdir(), "logs-test.zip");
    fs.writeFileSync(file, zip);
    const out = execFileSync("python3", ["-I", "-c",
      "import zipfile,sys,json\nz=zipfile.ZipFile(sys.argv[1])\nassert z.testzip() is None\n" +
      "print(json.dumps({i.filename:[z.read(i).decode(),list(i.date_time)] for i in z.infolist()}))",
      file]).toString();
    const read = JSON.parse(out);
    for (const e of entries) {
      assert.strictEqual(read[e.name][0], Buffer.from(e.bytes).toString());
      assert.deepStrictEqual(read[e.name][1], [2026, 11, 26, 14, 30, 10]);
    }
    assert(compression ? zip.length < 2000 : zip.length > 70000);
  }
  const { logZipSelect } = make()(CRC32_TABLE, undefined);
  const now = 1795700000;
  const data = { now, boot: 7, files: [
    { path: "/logs/debug/20261120-b00002-p001.log", modified: now - 6 * 86400 },
    { path: "/logs/debug/20261126-b00006-p001.log", modified: now - 3600 },
    { path: "/logs/debug/00000000-b00005-p001.log", modified: 100 },
    { path: "/logs/debug/20261124-b00007-p001.log", modified: now - 2 * 86400 },
    { path: "/logs/battery.prev.csv", modified: now - 9 * 86400 },
  ] };
  assert.strictEqual(logZipSelect(data, false, now).length, 5);
  assert.deepStrictEqual(logZipSelect(data, true, now).map((f) => f.path.split("/").pop()),
    ["battery.prev.csv", "00000000-b00005-p001.log", "20261124-b00007-p001.log",
     "20261126-b00006-p001.log"]);
  console.log("log zip: all tests passed");
})().catch((e) => { console.error(e); process.exit(1); });
