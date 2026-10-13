export interface FormatDescriptor {
    extension: string;
    category: 'cad' | 'media' | 'docs' | 'archive';
    label: string;
    workerAction?: string;
    thumbnailMode: string;
}

export interface PreviewContext<T = unknown> {
    filePath: string;
    fileUrl: string;
    fileSize: number;
    extraMetadata?: T;
}

export type PreviewMountFn = (container: HTMLElement, ctx: PreviewContext) => Promise<void>;
