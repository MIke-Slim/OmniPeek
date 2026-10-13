package com.omnipeek.core;

import java.util.List;
import java.util.stream.Collectors;

public record PluginDescriptor(String id, String name, List<String> extensions) {
    public static void main(String[] args) {
        var plugin = new PluginDescriptor("cad-3d", "Industrial CAD", List.of(".step", ".stl", ".glb"));
        System.out.println("Registered Plugin: " + plugin.name() + " for " + plugin.extensions());
    }
}
