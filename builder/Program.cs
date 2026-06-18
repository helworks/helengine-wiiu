namespace helengine.wiiu.builder;

/// <summary>
/// Provides a small command-line entrypoint for the Wii U builder assembly.
/// </summary>
public static class Program {
    /// <summary>
    /// Runs the builder smoke mode or prints the builder identity.
    /// </summary>
    /// <param name="args">Command-line arguments.</param>
    /// <returns>Zero on success.</returns>
    public static int Main(string[] args) {
        if (args.Length > 0 && string.Equals(args[0], "--describe", StringComparison.OrdinalIgnoreCase)) {
            Console.WriteLine("helengine.wiiu.builder");
            Console.WriteLine("wiiu");
            Console.WriteLine("Nintendo Wii U");
            return 0;
        }

        if (args.Length > 0 && string.Equals(args[0], "--smoke-test", StringComparison.OrdinalIgnoreCase)) {
            Console.WriteLine("wiiu.builder smoke test entrypoint");
            return 0;
        }

        Console.WriteLine("helengine.wiiu.builder --describe | --smoke-test");
        return 0;
    }
}
