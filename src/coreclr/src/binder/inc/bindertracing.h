// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.
//
// bindertracing.h
//

#ifndef __BINDER_TRACING_H__
#define __BINDER_TRACING_H__

class Assembly;
class AssemblySpec;
class PEAssembly;

namespace BinderTracing
{
    bool IsEnabled();

    // If tracing is enabled, this class fires an assembly bind start event on construction
    // and the corresponding stop event on destruction
    class AssemblyBindOperation
    {
    public:
        // This class assumes the assembly spec will have a longer lifetime than itself
        AssemblyBindOperation(AssemblySpec *assemblySpec, const WCHAR *assemblyPath = nullptr);
        ~AssemblyBindOperation();

        void SetResult(PEAssembly *assembly, bool cached = false);

        struct BindRequest
        {
            AssemblySpec *AssemblySpec;
            SString AssemblyName;
            SString AssemblyPath;
            SString RequestingAssembly;
            SString AssemblyLoadContext;
            SString RequestingAssemblyLoadContext;
        };

    private:
        bool ShouldIgnoreBind();

    private:
        BindRequest m_bindRequest;
        bool m_populatedBindRequest;

        bool m_checkedIgnoreBind;
        bool m_ignoreBind;

        PEAssembly *m_resultAssembly;
        bool m_cached;
    };

    // An object of this class manages firing events for all the stages during a binder resolving
    // attempt operation.  It has minimal cost if tracing for this event is disabled.
    //
    // This class should be declared in the stack.  As information is determined by each stage
    // (e.g. an AssemblySpec is initialized), the appropriate Set*() method should be called.  All
    // pointers held by an object of this class must either be a nullptr, or point to a valid
    // object during the time it is in scope.
    //
    // As the resolution progresses to different stages, the GoToStage() method should be called.
    // Calling it will fire an event for the previous stage with whatever context the class had
    // at that point; it is assumed that if GoToStage() is called, the previous stage failed
    // (the HRESULT is read by the dtor to assess success).
    //
    // It holds a reference to a HRESULT (that must be live during the lifetime of this object),
    // which is used to determine the success or failure of a stage either at the moment this
    // class is destructed (e.g. last stage), or when moving from one stage to another.  (This
    // is especially useful if the HRESULT is captured from an exception handler.)
    class ResolutionAttemptedOperation
    {
    public:
        enum class Stage : uint16_t
        {
            FindInLoadContext = 0,
            AssemblyLoadContextLoad = 1,
            PlatformAssemblies = 2,
            DefaultAssemblyLoadContextFallback = 3,
            AssemblyLoadContextResolvingEvent = 4,
            AppDomainAssemblyResolveEvent = 5,
            NotYetStarted = 0xffff, // Used as flag to not fire event; not present in value map
        };

    private:
#ifdef FEATURE_EVENT_TRACE
        enum class Result : uint16_t
        {
            Success = 0,
            AssemblyNotFound = 1,
            IncompatbileVersion = 2,
            MismatchedAssemblyName = 3,
            Failure = 4,
        };

        // A reference to an HRESULT stored in the same scope as this object lets
        // us determine if the last requested stage was successful or not, regardless
        // if it was set through a function call (e.g. BindAssemblyByNameWorker()), or
        // if an exception was thrown and captured by the EX_CATCH_HRESULT() macro.
        const HRESULT &m_hr;

        Stage m_stage;

        BINDER_SPACE::AssemblyName *m_pAssemblyName;
        BINDER_SPACE::Assembly *m_pFoundAssembly;

        INT_PTR m_pManagedALC;

        bool m_tracingEnabled;

        void TraceStageEnd(Stage resStage);
#endif // FEATURE_EVENT_TRACE

    public:
        ResolutionAttemptedOperation(const HRESULT& hr)
#ifdef FEATURE_EVENT_TRACE
            : m_hr{hr}
            , m_lastStage{Stage::NotYetStarted}
            , m_pAssemblyName{nullptr}
            , m_pFoundAssembly{nullptr}
            , m_pManagedALC{0}
            , m_tracingEnabled{EventEnabledResolutionAttempted()}
#endif // FEATURE_EVENT_TRACE
        {
        }

        void SetAssemblyName(BINDER_SPACE::AssemblyName *assemblyName)
        {
#ifdef FEATURE_EVENT_TRACE
            m_pAssemblyName = assemblyName;
#endif // FEATURE_EVENT_TRACE
        }

        void SetManagedALC(INT_PTR managedALC)
        {
#ifdef FEATURE_EVENT_TRACE
            m_pManagedALC = managedALC;
#endif // FEATURE_EVENT_TRACE
        }

        void SetFoundAssembly(BINDER_SPACE::Assembly *assembly)
        {
#ifdef FEATURE_EVENT_TRACE
            m_pFoundAssembly = assembly;
#endif // FEATURE_EVENT_TRACE
        }

        void GoToStage(Stage stage)
        {
#ifdef FEATURE_EVENT_TRACE
            if (m_tracingEnabled)
            {
                assert(m_stage != stage);
                assert(stage != Stage::NotYetStarted);

                // Going to a different stage should only happen if the current
                // stage failed (or if the binding process wasn't yet started).
                // Firing the event at this point not only helps timing each binding
                // stage, but avoids keeping track of which stages were reached to
                // resolve the assembly.
                TraceStageEnd();
                m_stage = stage;
            }
#endif // FEATURE_EVENT_TRACE
        }

#ifdef FEATURE_EVENT_TRACE
        ~ResolutionAttemptedOperation()
        {
            if (m_tracingEnabled)
            {
                TraceStageEnd();
            }
        }
#endif // FEATURE_EVENT_TRACE
    };

    // This must match the BindingPathSource value map in ClrEtwAll.man
    enum PathSource
    {
        ApplicationAssemblies,
        AppNativeImagePaths,
        AppPaths,
        PlatformResourceRoots,
        SatelliteSubdirectory
    };

    void PathProbed(const WCHAR *path, PathSource source, HRESULT hr);
};

#endif // __BINDER_TRACING_H__
