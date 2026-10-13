/**
 * OmniPeek Front-end Interactive Controller
 */
export class OmniViewController {
    constructor(container) {
        this.container = container;
        this.activePlugin = null;
    }

    async loadPreview(fileUrl, ext) {
        console.log(`[OmniPeek] Loading ${fileUrl} (${ext})...`);
        const plugin = window.OmniPluginRegistry ? window.OmniPluginRegistry[ext] : null;
        if (plugin && typeof plugin.mount === 'function') {
            await plugin.mount(this.container, { fileUrl, ext });
        }
    }
}
