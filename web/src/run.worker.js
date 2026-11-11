self.onmessage = ({ data: { code, input } }) => {
    self.$write = (text) => self.postMessage({ type: "out", text });
    self.$error = (text) => self.postMessage({ type: "error", text: text + "\n" });
    self.$input = input;
    self.process = { exitCode: 0 };
    try {
        new Function(code)();
    } catch (error) {
        self.$error(String(error));
        self.process.exitCode = 1;
    }
    self.postMessage({ type: "done", exitCode: self.process.exitCode });
};
