import { CSRFContext, CSRFProvider } from "../providers/CSRFProvider"
import { Route, MemoryRouter } from "react-router-dom"
import Login from "./pages/Login"
import SignUp from "./pages/SignUp"
import { useCookies } from "react-cookie"
import { Navigate, Outlet } from 'react-router-dom';
import DirectoryManager from "./pages/DirectoryManager"
import { FileTreeProvider } from "../providers/FileTreeProvider"
import { AddFileModalProvider } from "../providers/AddFileModalProvider"

const ProtectedRoute = ({ isAuthenticated }: { isAuthenticated: any }) => {
    if (!isAuthenticated) {
        return <Navigate to="/login" replace />;
    }
    return <Outlet />;
};

export default function App() {

    let [cookies, setCookie, removeCookie] = useCookies()

    return <MemoryRouter>
        <CSRFProvider>
            <FileTreeProvider>
                <AddFileModalProvider>
                    <Route element={<Login />} path="/login" />
                    <Route element={<SignUp />} path="/signup" />
                    <Route index={undefined} element={<ProtectedRoute isAuthenticated={cookies.username} />} path="/main">
                        <DirectoryManager username={cookies.username} />
                    </Route>
                </AddFileModalProvider>
            </FileTreeProvider>
        </CSRFProvider>
    </MemoryRouter>

}
