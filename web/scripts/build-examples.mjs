import { execFileSync } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const EXAMPLES = ["factorial", "primes", "dangling_else", "types", "input"];

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const bin = ["minicpp.exe", "minicpp"].map((f) => join(root, f)).find(existsSync);
if (!bin) {
    console.error("Не знайдено minicpp. Спочатку зберіть проєкт: build.bat або make");
    process.exit(1);
}

mkdirSync(join(root, "build"), { recursive: true });

const bundles = EXAMPLES.map((name) => {
    const out = join(root, "build", `${name}.bundle.json`);
    execFileSync(bin, [join(root, "examples", `${name}.cpp`), "--bundle", out]);
    const bundle = JSON.parse(readFileSync(out, "utf8"));
    const input = join(root, "examples", `${name}.in`);
    if (existsSync(input)) bundle.stdin = readFileSync(input, "utf8").trim();
    return bundle;
});

const json = "[\n" + bundles.map((b) => JSON.stringify(b)).join(",\n") + "\n]\n";
writeFileSync(join(root, "web", "src", "examples.json"), json);
console.log(`Записано ${bundles.length} прикладів у web/src/examples.json`);
