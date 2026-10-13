use std::path::Path;

pub enum PluginCategory {
    Cad3D,
    MediaPlayer,
    DevDocs,
    ArchiveFont,
}

pub struct FormatRegistry {
    pub name: String,
    pub category: PluginCategory,
}

impl FormatRegistry {
    pub fn new(name: &str, category: PluginCategory) -> Self {
        Self { name: name.to_string(), category }
    }
}

fn main() {
    let reg = FormatRegistry::new("OmniPeek Fast Rust Service", PluginCategory::Cad3D);
    println!("Initialized: {}", reg.name);
}
