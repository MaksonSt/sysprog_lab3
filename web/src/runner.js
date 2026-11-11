const TIMEOUT_MS = 5000;

export function runJs(code, input, { onOutput, onDone }) {
    const worker = new Worker(new URL("./run.worker.js", import.meta.url), { type: "module" });
    const started = performance.now();

    const finish = (exitCode, timedOut = false) => {
        clearTimeout(timer);
        worker.terminate();
        onDone({ exitCode, timedOut, ms: Math.round(performance.now() - started) });
    };
    const timer = setTimeout(() => finish(null, true), TIMEOUT_MS);

    worker.onmessage = ({ data }) => {
        if (data.type === "done") finish(data.exitCode);
        else onOutput(data.text, data.type === "error");
    };
    worker.onerror = (event) => {
        event.preventDefault();
        onOutput(event.message + "\n", true);
        finish(1);
    };
    worker.postMessage({ code, input });

    return () => {
        clearTimeout(timer);
        worker.terminate();
    };
}

export const TIMEOUT_SECONDS = TIMEOUT_MS / 1000;
