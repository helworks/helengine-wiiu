namespace helengine.wiiu.builder;

/// <summary>Retains a fully validated package file in memory until every referenced source and workitem has succeeded.</summary>
internal sealed class WiiUPreparedPayload {
    /// <summary>Identifies the destination relative to the Wii U package content root.</summary>
    public string RelativePath { get; init; }
    /// <summary>Owns the complete serialized bytes read or cooked during package preflight.</summary>
    public byte[] Bytes { get; init; }
}
