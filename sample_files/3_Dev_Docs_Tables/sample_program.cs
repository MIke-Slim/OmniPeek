using System;
using System.Collections.Generic;
using System.Linq;

namespace OmniPeek.Core
{
    public record FormatInfo(string Extension, string Category, bool Supported);

    public class Program
    {
        public static void Main()
        {
            var formats = new List<FormatInfo>
            {
                new(".step", "CAD", true),
                new(".mp4", "Media", true),
                new(".md", "Docs", true)
            };

            var supported = formats.Where(f => f.Supported).ToList();
            Console.WriteLine($"OmniPeek C# Runner: {supported.Count} formats active.");
        }
    }
}
